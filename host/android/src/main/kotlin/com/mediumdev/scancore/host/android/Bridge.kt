package com.mediumdev.scancore.host.android

import android.app.Application
import android.content.Context
import android.webkit.CookieManager
import com.mediumdev.scancore.host.ExtensionHost
import com.mediumdev.scancore.host.HostError
import com.mediumdev.scancore.host.MemoStore
import com.mediumdev.scancore.host.RpcServer
import com.mediumdev.scancore.host.string
import eu.kanade.tachiyomi.AppInfo
import eu.kanade.tachiyomi.network.HostNetwork
import eu.kanade.tachiyomi.network.JavaScriptEngine
import eu.kanade.tachiyomi.network.NetworkHelper
import eu.kanade.tachiyomi.network.PersistentCookieJar
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.Json
import kotlinx.serialization.protobuf.ProtoBuf
import okhttp3.Cookie
import okhttp3.HttpUrl.Companion.toHttpUrlOrNull
import uy.kohesive.injekt.Injekt
import uy.kohesive.injekt.api.addSingleton
import uy.kohesive.injekt.api.addSingletonFactory
import java.io.File

// The extension host inside the Android app, spoken to by SCANCORE's C++
// over JNI with the same protocol as the desktop helper: send() takes a
// request line, and the answer comes back through the native reply().
object Bridge {
    private var server: RpcServer? = null

    @JvmStatic
    @Synchronized
    fun start(
        context: Context,
        root: String,
        userAgent: String,
        appVersion: String,
    ) {
        if (server != null) return
        val app = context.applicationContext as Application
        val folder = File(root).apply { mkdirs() }
        HostNetwork.userAgent = userAgent
        HostNetwork.cookieJar = PersistentCookieJar(File(folder, "cookies.json"))
        HostNetwork.cacheDir = File(folder, "cache")
        AppInfo.set(versionCode = 1, versionName = appVersion)
        Injekt.addSingleton<Application>(app)
        Injekt.addSingleton<Context>(app)
        Injekt.addSingletonFactory { NetworkHelper(app) }
        Injekt.addSingletonFactory { JavaScriptEngine(app) }
        Injekt.addSingletonFactory {
            Json {
                ignoreUnknownKeys = true
                explicitNulls = false
            }
        }
        Injekt.addSingletonFactory<ProtoBuf> { ProtoBuf }
        val host = ExtensionHost(AndroidLoader(app), MemoStore(File(folder, "memos.json")), HOST_VERSION, AndroidSettings(app))
        server = RpcServer({ method, params -> handle(host, method, params) })
    }

    // One request line; answered later through reply().
    @JvmStatic
    fun send(line: String) {
        val running = server ?: return reply("""{"error":{"message":"The extension host is not started.","status":503}}""")
        running.submit(line) { reply(it) }
    }

    @JvmStatic
    external fun reply(line: String)

    // On a phone the browser check runs in a web view; its cookies reach
    // extensions from Android's own cookie store.
    private suspend fun handle(
        host: ExtensionHost,
        method: String,
        params: JsonObject,
    ): JsonElement {
        if (method != "importWebCookies") return host.handle(method, params)
        val url = params.string("url").toHttpUrlOrNull() ?: throw HostError("That is not a link.", 400)
        val header = CookieManager.getInstance().getCookie(url.toString()).orEmpty()
        val cookies = header.split(';').mapNotNull { Cookie.parse(url, it.trim()) }
        HostNetwork.cookieJar.saveFromResponse(url, cookies)
        return JsonPrimitive(cookies.size)
    }

    const val HOST_VERSION = "1"
}
