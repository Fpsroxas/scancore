package com.mediumdev.scancore.host.android

import android.app.Application
import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.mediumdev.scancore.host.ExtensionHost
import com.mediumdev.scancore.host.HostError
import com.mediumdev.scancore.host.MemoStore
import eu.kanade.tachiyomi.network.HostNetwork
import eu.kanade.tachiyomi.network.JavaScriptEngine
import eu.kanade.tachiyomi.network.NetworkHelper
import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.long
import kotlinx.serialization.json.put
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith
import uy.kohesive.injekt.Injekt
import uy.kohesive.injekt.api.addSingleton
import uy.kohesive.injekt.api.addSingletonFactory
import java.io.File
import java.net.ServerSocket
import java.util.Collections
import kotlin.concurrent.thread

// The test extension inside a real Android: the package manager reads and
// verifies it, it loads without conversion, QuickJS runs its JavaScript,
// and it reads the fixture site served from this test.
@RunWith(AndroidJUnit4::class)
class AndroidHostTest {
    companion object {
        private lateinit var context: Context
        private lateinit var host: ExtensionHost
        private lateinit var apk: File
        private lateinit var signer: String
        private val requests = Collections.synchronizedList(mutableListOf<String>())

        @JvmStatic
        @BeforeClass
        fun start() {
            val instrumentation = InstrumentationRegistry.getInstrumentation()
            context = instrumentation.targetContext
            val assets = instrumentation.context.assets
            apk = File(context.filesDir, "testext.apk").apply { setWritable(true) }
            assets.open("testext.apk").use { input -> apk.outputStream().use { input.copyTo(it) } }
            apk.setReadOnly()
            signer = assets.open("signer.txt").bufferedReader().readText().trim()

            val server = ServerSocket(0, 50, java.net.InetAddress.getLoopbackAddress())
            thread(isDaemon = true) {
                while (true) {
                    val socket = server.accept()
                    thread(isDaemon = true) {
                        socket.use { s ->
                            val reader = s.getInputStream().bufferedReader()
                            val requestLine = reader.readLine() ?: return@use
                            val headers = generateSequence { reader.readLine()?.takeIf { it.isNotEmpty() } }.toList()
                            val target = requestLine.split(' ')[1]
                            requests.add(target)
                            val path = target.substringBefore('?')
                            val own = headers.any { it.equals("X-Test-Source: 1", ignoreCase = true) }
                            val file =
                                when {
                                    path == "/popular" -> "popular.json"
                                    path == "/search" -> "search.json"
                                    path == "/series/1" -> "series-1.json"
                                    path == "/series/1/chapters" -> "chapters.json"
                                    path == "/chapter/2/pages" -> "pages.json"
                                    path == "/page/2" -> "page-2.json"
                                    path.startsWith("/img/") && own -> "image.png"
                                    else -> null
                                }
                            val body = file?.let { assets.open("site/$it").readBytes() } ?: ByteArray(0)
                            val status = if (file == null) "404 Not Found" else "200 OK"
                            val type = if (file?.endsWith(".png") == true) "image/png" else "application/json"
                            s.getOutputStream().apply {
                                write("HTTP/1.1 $status\r\nContent-Type: $type\r\nContent-Length: ${body.size}\r\nConnection: close\r\n\r\n".toByteArray())
                                write(body)
                                flush()
                            }
                        }
                    }
                }
            }
            System.setProperty("scancore.testext.base", "http://fixture.test:${server.localPort}")
            HostNetwork.dns = HostNetwork.loopbackForTestDomains
            val app = context.applicationContext as Application
            Injekt.addSingleton<Application>(app)
            Injekt.addSingleton<Context>(app)
            Injekt.addSingletonFactory { NetworkHelper(app) }
            Injekt.addSingletonFactory { JavaScriptEngine(app) }
            Injekt.addSingletonFactory { Json { ignoreUnknownKeys = true } }
            host = ExtensionHost(AndroidLoader(app), MemoStore(null), "test", AndroidSettings(app))
        }
    }

    private fun base() =
        buildJsonObject {
            put("pkg", "com.mediumdev.scancore.testext")
            put("apk", apk.path)
            put("signer", signer)
        }

    private val sourceId: String by lazy {
        runBlocking { host.handle("load", base()) }.jsonArray.single().jsonObject["id"]!!.jsonPrimitive.content
    }

    private fun call(
        method: String,
        extra: Map<String, JsonElement> = emptyMap(),
    ) = runBlocking {
        host.handle(method, JsonObject(base() + mapOf("source" to kotlinx.serialization.json.JsonPrimitive(sourceId)) + extra))
    }

    private fun p(value: Any) =
        when (value) {
            is String -> kotlinx.serialization.json.JsonPrimitive(value)
            is Number -> kotlinx.serialization.json.JsonPrimitive(value)
            is Boolean -> kotlinx.serialization.json.JsonPrimitive(value)
            else -> value as JsonElement
        }

    @Test
    fun browsesAndSearches() {
        val popular = call("popular", mapOf("page" to p(1))).jsonObject
        assertEquals(2, popular["items"]!!.jsonArray.size)
        val chosen =
            buildJsonArray {
                add(buildJsonObject { put("type", "header") })
                add(buildJsonObject { put("state", 1) })
                add(buildJsonObject { put("state", true) })
            }
        call("search", mapOf("query" to p("one"), "filters" to chosen))
        val sent = requests.last { it.startsWith("/search") }
        assertTrue(sent, sent.contains("genre=Action") && sent.contains("adult=1"))
    }

    @Test
    fun detailsRunJavaScript() {
        val details = call("details", mapOf("url" to p("/series/1"))).jsonObject
        assertEquals("A test series. js-42", details["description"]!!.jsonPrimitive.content)
        val chapters = call("chapters", mapOf("url" to p("/series/1"))).jsonArray
        val expected = java.time.LocalDate.of(2026, 1, 15).atStartOfDay(java.time.ZoneId.systemDefault()).toInstant().toEpochMilli()
        assertEquals(expected, chapters[0].jsonObject["uploaded"]!!.jsonPrimitive.long)
    }

    @Test
    fun imagesThroughTheSourceClient() {
        val pages = call("pages", mapOf("url" to p("/chapter/2"))).jsonArray
        val target = File(context.cacheDir, "page-${System.nanoTime()}")
        call("fetchImage", mapOf("page" to pages[1], "target" to p(target.path)))
        val expected = InstrumentationRegistry.getInstrumentation().context.assets.open("site/image.png").readBytes()
        assertTrue(expected.contentEquals(target.readBytes()))
    }

    @Test
    fun settings() {
        val quality = call("preferences").jsonArray.single().jsonObject
        assertEquals("high", quality["value"]!!.jsonPrimitive.content)
        call("setPreference", mapOf("key" to p("quality"), "value" to p("low")))
        assertEquals("low", call("preferences").jsonArray.single().jsonObject["value"]!!.jsonPrimitive.content)
        call("popular", mapOf("page" to p(1)))
        assertTrue(requests.last { it.startsWith("/popular") }.contains("quality=low"))
        call("setPreference", mapOf("key" to p("quality"), "value" to p("high")))
    }

    @Test
    fun refusesAnotherSigner() {
        try {
            runBlocking {
                host.handle("load", JsonObject(base() + mapOf("pkg" to p("com.mediumdev.scancore.other"), "signer" to p("00".repeat(32)))))
            }
            throw AssertionError("loaded an extension signed with an untrusted key")
        } catch (e: HostError) {
            assertEquals(403, e.status)
        }
    }

    @Test
    fun callsAreQuick() {
        sourceId
        val started = System.nanoTime()
        repeat(100) { call("filters") }
        val perCall = (System.nanoTime() - started) / 100 / 1_000_000.0
        android.util.Log.i("AndroidHostTest", "filters call: %.2f ms".format(perCall))
        assertTrue("calls take $perCall ms", perCall < 20)
    }
}
