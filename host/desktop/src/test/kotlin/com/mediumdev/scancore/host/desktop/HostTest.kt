package com.mediumdev.scancore.host.desktop

import com.mediumdev.scancore.host.ExtensionHost
import com.mediumdev.scancore.host.HostError
import com.sun.net.httpserver.HttpServer
import eu.kanade.tachiyomi.network.HostNetwork
import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.long
import kotlinx.serialization.json.put
import org.junit.AfterClass
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.BeforeClass
import org.junit.Test
import java.io.File
import java.net.InetSocketAddress
import java.nio.file.Files
import java.util.Collections

// The test extension (host/testext), loaded the way SCANCORE's will be:
// verified, converted from Dalvik bytecode, run against a fixture site on
// fixture.test (tests/fixtures/host).
class HostTest {
    companion object {
        private val apk = File(System.getProperty("scancore.testext.apk", ""))
        private val signer = File(apk.parentFile, "signer.txt").takeIf { it.exists() }?.readText()?.trim()
        private val fixtures = File(System.getProperty("scancore.fixtures", ""))
        private lateinit var server: HttpServer
        private lateinit var host: ExtensionHost
        private val requests = Collections.synchronizedList(mutableListOf<String>())

        @JvmStatic
        @BeforeClass
        fun start() {
            assumeTrue("No test extension APK (no Android SDK)", apk.isFile && signer != null)
            server = HttpServer.create(InetSocketAddress("127.0.0.1", 0), 0)
            server.createContext("/") { exchange ->
                val path = exchange.requestURI.path
                requests.add(exchange.requestURI.toString())
                val (status, file) =
                    when {
                        path == "/popular" -> 200 to "popular.json"
                        path == "/search" -> 200 to "search.json"
                        path == "/series/1" -> 200 to "series-1.json"
                        path == "/series/1/chapters" -> 200 to "chapters.json"
                        path == "/chapter/2/pages" -> 200 to "pages.json"
                        path == "/page/2" -> 200 to "page-2.json"
                        // Only the extension's own client sends this header.
                        path.startsWith("/img/") && exchange.requestHeaders.getFirst("X-Test-Source") == "1" -> 200 to "image.png"
                        else -> 404 to null
                    }
                val body = file?.let { File(fixtures, it).readBytes() } ?: ByteArray(0)
                exchange.responseHeaders.add("Content-Type", if (file?.endsWith(".png") == true) "image/png" else "application/json")
                exchange.sendResponseHeaders(status, if (body.isEmpty()) -1 else body.size.toLong())
                exchange.responseBody.use { it.write(body) }
            }
            server.start()
            System.setProperty("scancore.testext.base", "http://fixture.test:${server.address.port}")
            val root = Files.createTempDirectory("scancore-host-test").toFile()
            host = setUp(root, null, "test").first
            HostNetwork.dns = HostNetwork.loopbackForTestDomains
        }

        @JvmStatic
        @AfterClass
        fun stop() {
            if (::server.isInitialized) server.stop(0)
        }
    }

    private fun call(
        method: String,
        extra: Map<String, Any?> = emptyMap(),
    ) = runBlocking {
        host.handle(
            method,
            buildJsonObject {
                put("pkg", "com.mediumdev.scancore.testext")
                put("apk", apk.path)
                put("signer", signer)
                put("source", sourceId)
                for ((k, v) in extra) {
                    when (v) {
                        is String -> put(k, v)
                        is Int -> put(k, v)
                        is JsonArray -> put(k, v)
                        is JsonObject -> put(k, v)
                    }
                }
            },
        )
    }

    private val sourceId: String by lazy {
        runBlocking {
            host
                .handle(
                    "load",
                    buildJsonObject {
                        put("pkg", "com.mediumdev.scancore.testext")
                        put("apk", apk.path)
                        put("signer", signer)
                    },
                ).jsonArray
                .single()
                .jsonObject["id"]!!
                .jsonPrimitive.content
        }
    }

    @Test
    fun loadsAndBrowses() {
        val loaded = runBlocking { host.handle("load", buildJsonObject { put("pkg", "com.mediumdev.scancore.testext"); put("apk", apk.path); put("signer", signer) }) }
        val source = loaded.jsonArray.single().jsonObject
        assertEquals("Test Source", source["name"]!!.jsonPrimitive.content)
        assertEquals("en", source["lang"]!!.jsonPrimitive.content)
        assertEquals("true", source["configurable"]!!.jsonPrimitive.content)
        val popular = call("popular", mapOf("page" to 1)).jsonObject
        assertEquals(2, popular["items"]!!.jsonArray.size)
        assertEquals("true", popular["hasNextPage"]!!.jsonPrimitive.content)
        assertEquals("/series/1", popular["items"]!!.jsonArray[0].jsonObject["url"]!!.jsonPrimitive.content)
    }

    @Test
    fun searchSendsFilters() {
        val filters = call("filters").jsonArray
        assertEquals(listOf("header", "select", "checkbox"), filters.map { it.jsonObject["type"]!!.jsonPrimitive.content })
        val chosen =
            buildJsonArray {
                add(filters[0])
                add(buildJsonObject { put("type", "select"); put("state", 2) })
                add(buildJsonObject { put("type", "checkbox"); put("state", true) })
            }
        call("search", mapOf("query" to "one", "filters" to chosen))
        val sent = requests.last { it.startsWith("/search") }
        assertTrue(sent, sent.contains("q=one") && sent.contains("genre=Drama") && sent.contains("adult=1"))
    }

    @Test
    fun detailsChaptersAndPages() {
        val details = call("details", mapOf("url" to "/series/1")).jsonObject
        assertEquals("A. Writer", details["author"]!!.jsonPrimitive.content)
        // JavaScript ran (Rhino standing in for QuickJS).
        assertEquals("A test series. js-42", details["description"]!!.jsonPrimitive.content)
        assertEquals(listOf("Action", "Drama"), details["genres"]!!.jsonArray.map { it.jsonPrimitive.content })

        val chapters = call("chapters", mapOf("url" to "/series/1")).jsonArray
        assertEquals(2, chapters.size)
        // Dates went through the Android-behaving SimpleDateFormat, in the
        // device's time zone as on a phone.
        val expected = java.time.LocalDate.of(2026, 1, 15).atStartOfDay(java.time.ZoneId.systemDefault()).toInstant().toEpochMilli()
        assertEquals(expected, chapters[0].jsonObject["uploaded"]!!.jsonPrimitive.long)

        val pages = call("pages", mapOf("url" to "/chapter/2")).jsonArray
        assertEquals(2, pages.size)
        assertTrue(pages[0].jsonObject["imageUrl"]!!.jsonPrimitive.content.endsWith("/img/1.png"))
        assertEquals("", pages[1].jsonObject["imageUrl"]!!.jsonPrimitive.content)

        // The second page needs a request for its image; the image comes
        // through the extension's client straight into the given file.
        val target = Files.createTempFile("page", ".img").toFile()
        val fetched = call("fetchImage", mapOf("page" to pages[1].jsonObject, "target" to target.path)).jsonObject
        assertEquals("image/png", fetched["contentType"]!!.jsonPrimitive.content)
        assertEquals(File(fixtures, "image.png").readBytes().toList(), target.readBytes().toList())
    }

    @Test
    fun settings() {
        val prefs = call("preferences").jsonArray
        val quality = prefs.single().jsonObject
        assertEquals("list", quality["type"]!!.jsonPrimitive.content)
        assertEquals("high", quality["value"]!!.jsonPrimitive.content)
        call("setPreference", mapOf("key" to "quality", "value" to JsonPrimitive("low").content))
        assertEquals("low", call("preferences").jsonArray.single().jsonObject["value"]!!.jsonPrimitive.content)
        call("popular", mapOf("page" to 1))
        assertTrue(requests.last { it.startsWith("/popular") }.contains("quality=low"))
        call("setPreference", mapOf("key" to "quality", "value" to "high"))
    }

    @Test
    fun refusesAnotherSigner() {
        try {
            runBlocking {
                // A fresh pkg name so the cached, verified jar is not reused.
                val copy = File(apk.parentFile, "copy-${System.nanoTime()}.apk")
                apk.copyTo(copy)
                host.handle("load", buildJsonObject { put("pkg", "com.mediumdev.scancore.other"); put("apk", copy.path); put("signer", "00".repeat(32)) })
            }
            throw AssertionError("loaded an extension signed with an untrusted key")
        } catch (e: HostError) {
            assertEquals(403, e.status)
        }
    }

    @Test
    fun loadIsCachedAndQuick() {
        sourceId
        val started = System.nanoTime()
        repeat(100) { call("filters") }
        val perCall = (System.nanoTime() - started) / 100 / 1_000_000.0
        println("filters call: %.2f ms".format(perCall))
        assertTrue("calls take $perCall ms", perCall < 20)
    }
}
