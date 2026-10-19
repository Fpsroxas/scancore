package com.mediumdev.scancore.host

import eu.kanade.tachiyomi.network.HostNetwork
import eu.kanade.tachiyomi.network.MemoryCookieJar
import eu.kanade.tachiyomi.source.CatalogueSource
import eu.kanade.tachiyomi.source.ConfigurableSource
import eu.kanade.tachiyomi.source.Source
import eu.kanade.tachiyomi.source.model.SChapter
import eu.kanade.tachiyomi.source.model.SManga
import eu.kanade.tachiyomi.source.online.HttpSource
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import okhttp3.Cookie
import okhttp3.HttpUrl.Companion.toHttpUrlOrNull
import java.io.File
import java.util.concurrent.ConcurrentHashMap

// Loads extensions on first use (desktop: converted APKs; Android: the
// package manager) and reports what they contain.
interface ExtensionLoader {
    // The extension's sources. `signer` is the SHA-256 of the certificate
    // SCANCORE trusted; the loader refuses a file signed otherwise.
    fun load(
        pkg: String,
        path: String,
        signer: String?,
    ): List<Source>

    // Lib versions this host runs (e.g. "1.4", "1.5", "1.6").
    val libs: List<String>
}

// Answers SCANCORE's calls for Mihon extensions. Every call names its
// extension (pkg, apk, signer) so the host can load it on demand, and its
// source by id.
class ExtensionHost(
    private val loader: ExtensionLoader,
    private val memos: MemoStore,
    private val version: String,
    private val settings: SettingsBridge,
) {
    private val loaded = ConcurrentHashMap<String, Map<Long, Source>>()
    private val locks = ConcurrentHashMap<String, Mutex>()

    suspend fun handle(
        method: String,
        params: JsonObject,
    ): JsonElement =
        when (method) {
            "hello" ->
                buildJsonObject {
                    put("version", version)
                    put("libs", buildJsonArray { loader.libs.forEach { add(JsonPrimitive(it)) } })
                }
            "load" -> describe(sources(params))
            "unload" -> {
                loaded.remove(params.string("pkg"))
                JsonNull
            }
            "setUserAgent" -> {
                HostNetwork.userAgent = params.string("userAgent")
                JsonNull
            }
            "setCookies" -> setCookies(params)
            else -> call(method, params)
        }

    private suspend fun sources(params: JsonObject): Map<Long, Source> {
        val pkg = params.string("pkg")
        loaded[pkg]?.let { return it }
        val lock = locks.getOrPut(pkg) { Mutex() }
        return lock.withLock {
            loaded[pkg] ?: loader
                .load(pkg, params.string("apk"), params.optString("signer"))
                .associateBy { it.id }
                .also { loaded[pkg] = it }
        }
    }

    private fun describe(sources: Map<Long, Source>) =
        buildJsonArray {
            for (source in sources.values) {
                add(
                    buildJsonObject {
                        put("id", source.id.toString())
                        put("name", source.name)
                        put("lang", source.lang)
                        put("baseUrl", (source as? HttpSource)?.baseUrl)
                        put("supportsLatest", (source as? CatalogueSource)?.supportsLatest ?: false)
                        put("configurable", source is ConfigurableSource)
                    },
                )
            }
        }

    private suspend fun source(params: JsonObject): Source {
        val id = params.string("source").toLongOrNull() ?: throw HostError("That is not a source id.", 400)
        return sources(params)[id] ?: throw HostError("The extension has no source $id. It may need updating.", 404)
    }

    private fun base(source: Source) = (source as? HttpSource)?.baseUrl

    private fun catalogue(source: Source) = source as? CatalogueSource ?: throw HostError("${source.name} cannot be browsed.", 501)

    private fun manga(
        source: Source,
        url: String,
    ): SManga =
        SManga.create().also {
            it.url = url
            memos.manga(source.id, url)?.let { memo -> it.memo = memo }
        }

    private fun chapter(
        source: Source,
        url: String,
    ): SChapter =
        SChapter.create().also {
            it.url = url
            memos.chapter(source.id, url)?.let { memo -> it.memo = memo }
        }

    private suspend fun call(
        method: String,
        params: JsonObject,
    ): JsonElement {
        val source = source(params)
        val page = params.int("page", 1)
        return when (method) {
            "popular" -> Codec.mangas(catalogue(source).getPopularManga(page).also { remember(source, it.mangas) }, base(source))
            "latest" -> Codec.mangas(catalogue(source).getLatestUpdates(page).also { remember(source, it.mangas) }, base(source))
            "search" -> {
                val filters = Codec.applyFilters(source.getFilterList(), params["filters"] as? JsonArray ?: JsonArray(emptyList()))
                Codec.mangas(
                    catalogue(source).getSearchManga(page, params.optString("query").orEmpty(), filters).also { remember(source, it.mangas) },
                    base(source),
                )
            }
            "details" -> {
                val update = source.getMangaUpdate(manga(source, params.string("url")), emptyList(), fetchDetails = true, fetchChapters = false)
                remember(source, listOf(update.manga))
                Codec.details(update.manga, base(source))
            }
            "chapters" -> {
                val update = source.getMangaUpdate(manga(source, params.string("url")), emptyList(), fetchDetails = false, fetchChapters = true)
                for (c in update.chapters) if (c.memo.isNotEmpty()) memos.putChapter(source.id, c.url, c.memo)
                Codec.chapters(update.chapters)
            }
            "pages" -> Codec.pages(source.getPageList(chapter(source, params.string("url"))))
            "imageUrl" -> {
                val http = source as? HttpSource ?: throw HostError("${source.name} has no image addresses.", 501)
                JsonPrimitive(http.getImageUrl(Codec.page(params["page"]?.jsonObject ?: JsonObject(emptyMap()))))
            }
            "fetchImage" -> fetchImage(source, params)
            "filters" -> Codec.filters(source.getFilterList())
            "preferences" -> settings.describe(configurable(source))
            "setPreference" -> {
                settings.set(configurable(source), params.string("key"), params["value"] ?: JsonNull)
                JsonNull
            }
            else -> throw HostError("The host does not know \"$method\".", 400)
        }
    }

    // The image through the source's own client (its rate limits, headers
    // and any interceptor that unscrambles pages), written straight to the
    // file SCANCORE's image cache names; the bytes never cross the pipe.
    private suspend fun fetchImage(
        source: Source,
        params: JsonObject,
    ): JsonElement {
        val http = source as? HttpSource ?: throw HostError("${source.name} has no images.", 501)
        val target = File(params.string("target"))
        var page = Codec.page(params["page"]?.jsonObject ?: JsonObject(emptyMap()))
        if (page.imageUrl.isNullOrEmpty()) page.imageUrl = http.getImageUrl(page)
        http.getImage(page).use { response ->
            if (!response.isSuccessful) throw HostError("The site answered HTTP ${response.code} for an image.", response.code)
            target.parentFile?.mkdirs()
            val temp = File(target.path + ".part")
            response.body.byteStream().use { input -> temp.outputStream().use { input.copyTo(it) } }
            if (!temp.renameTo(target)) {
                target.delete()
                temp.renameTo(target)
            }
            return buildJsonObject {
                put("contentType", response.header("Content-Type"))
                put("size", target.length())
            }
        }
    }

    private fun configurable(source: Source) = source as? ConfigurableSource ?: throw HostError("${source.name} has no settings.", 501)

    private fun remember(
        source: Source,
        mangas: List<SManga>,
    ) {
        for (m in mangas) if (m.memo.isNotEmpty()) memos.putManga(source.id, m.url, m.memo)
    }

    // Cookies from SCANCORE's browser-check window: {url, cookies: ["a=b; Path=/", ...]}.
    private fun setCookies(params: JsonObject): JsonElement {
        val url = params.string("url").toHttpUrlOrNull() ?: throw HostError("That is not a link.", 400)
        val cookies = (params["cookies"] as? JsonArray).orEmpty().mapNotNull { Cookie.parse(url, it.jsonPrimitive.content) }
        val jar = HostNetwork.cookieJar
        if (jar is MemoryCookieJar) jar.add(cookies) else jar.saveFromResponse(url, cookies)
        return JsonPrimitive(cookies.size)
    }
}

// A source's settings screen as JSON, and changes to it, with the platform's
// preference classes (AndroidCompat's on desktop, androidx on Android).
//   [{key, title, summary, enabled, type: switch|list|multiselect|text|info,
//     entries, values, value}]
interface SettingsBridge {
    fun describe(source: ConfigurableSource): JsonArray

    fun set(
        source: ConfigurableSource,
        key: String,
        value: JsonElement,
    )
}
