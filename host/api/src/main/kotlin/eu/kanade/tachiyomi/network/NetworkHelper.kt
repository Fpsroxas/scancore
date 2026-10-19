// SCANCORE's NetworkHelper, with the members Mihon's has (client,
// cloudflareClient, defaultUserAgentProvider) so extensions work unchanged.
// Browser checks are not solved here: they are reported to SCANCORE, which
// opens the site in its own window and hands the cookies back.
package eu.kanade.tachiyomi.network

import android.content.Context
import eu.kanade.tachiyomi.network.interceptor.UncaughtExceptionInterceptor
import eu.kanade.tachiyomi.network.interceptor.UserAgentInterceptor
import okhttp3.Cache
import okhttp3.Interceptor
import okhttp3.OkHttpClient
import okhttp3.Response
import okhttp3.brotli.BrotliInterceptor
import java.io.File
import java.util.concurrent.TimeUnit

class NetworkHelper(
    @Suppress("UNUSED_PARAMETER") context: Context,
) {
    val cookieJar get() = HostNetwork.cookieJar

    val client: OkHttpClient by lazy {
        OkHttpClient
            .Builder()
            .cookieJar(HostNetwork.cookieJar)
            .dns(HostNetwork.dns)
            .connectTimeout(30, TimeUnit.SECONDS)
            .readTimeout(30, TimeUnit.SECONDS)
            .callTimeout(2, TimeUnit.MINUTES)
            .apply {
                HostNetwork.cacheDir?.let { cache(Cache(File(it, "network_cache"), 5L * 1024 * 1024)) }
            }.addInterceptor(UncaughtExceptionInterceptor())
            .addInterceptor(UserAgentInterceptor(::defaultUserAgentProvider))
            .addInterceptor(BrotliInterceptor)
            .addInterceptor(BrowserCheckInterceptor())
            .build()
    }

    @Deprecated("The regular client handles Cloudflare by default")
    @Suppress("UNUSED")
    val cloudflareClient: OkHttpClient get() = client

    fun defaultUserAgentProvider() = HostNetwork.userAgent
}

// A Cloudflare (or similar) interstitial: reported, not solved.
class BrowserCheckInterceptor : Interceptor {
    override fun intercept(chain: Interceptor.Chain): Response {
        val response = chain.proceed(chain.request())
        if (response.code in setOf(403, 503, 429) && response.header("Server")?.startsWith("cloudflare", true) == true) {
            val challenge =
                response.header("cf-mitigated") == "challenge" ||
                    response.peekBody(8192).string().let { "Just a moment" in it || "challenge-platform" in it }
            if (challenge) {
                response.close()
                val url = chain.request().url
                throw BrowserCheckException("${url.scheme}://${url.host}/")
            }
        }
        return response
    }
}
