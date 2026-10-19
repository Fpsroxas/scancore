// SCANCORE: what the host sets up before any extension loads: the browser
// identity SCANCORE uses (so cookies from its browser-check window stay
// valid), where cookies and the HTTP cache live.
package eu.kanade.tachiyomi.network

import okhttp3.CookieJar
import okhttp3.Dns
import java.io.File
import java.net.InetAddress

object HostNetwork {
    @Volatile
    var userAgent: String =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36"

    @Volatile
    var cookieJar: CookieJar = MemoryCookieJar()

    @Volatile
    var cacheDir: File? = null

    // Tests serve fixture sites on *.test from this machine.
    @Volatile
    var dns: Dns = Dns.SYSTEM

    val loopbackForTestDomains =
        Dns { host ->
            if (host.endsWith(".test")) listOf(InetAddress.getLoopbackAddress()) else Dns.SYSTEM.lookup(host)
        }
}

// Thrown when a site answers with a browser check SCANCORE's window can pass.
class BrowserCheckException(
    val url: String,
) : java.io.IOException("$url asked for a browser check.")
