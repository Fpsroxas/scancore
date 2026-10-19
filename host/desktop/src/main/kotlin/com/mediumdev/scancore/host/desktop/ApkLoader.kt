package com.mediumdev.scancore.host.desktop

import com.android.apksig.ApkVerifier
import com.googlecode.d2j.dex.Dex2jar
import com.googlecode.d2j.reader.MultiDexFileReader
import com.googlecode.dex2jar.tools.BaksmaliBaseDexExceptionHandler
import com.mediumdev.scancore.host.ExtensionLoader
import com.mediumdev.scancore.host.HostError
import eu.kanade.tachiyomi.source.Source
import eu.kanade.tachiyomi.source.SourceFactory
import net.dongliu.apk.parser.ApkFile
import org.w3c.dom.Element
import java.io.File
import java.net.URLClassLoader
import java.security.MessageDigest
import javax.xml.parsers.DocumentBuilderFactory

// Runs Mihon extension APKs on the JVM: checks the signature (apksig) is the
// one SCANCORE trusted, converts the Dalvik bytecode to JVM bytecode
// (dex2jar, as Suwayomi does), points date parsing at Android's behaviour,
// and loads the sources in their own class loader. The check and the
// conversion happen once per APK version; later starts load the cached jar.
class ApkLoader(
    private val cacheDir: File,
) : ExtensionLoader {
    override val libs = listOf("1.4", "1.5", "1.6")

    companion object {
        // Raised whenever conversion changes (BytecodeEditor included).
        const val CONVERTER = 1
    }

    override fun load(
        pkg: String,
        path: String,
        signer: String?,
    ): List<Source> {
        val apk = File(path)
        if (!apk.isFile) throw HostError("The extension's file is missing. Install it again.", 404)
        val bytes = apk.readBytes()
        // A new converter makes older conversions stale.
        val stamp = "${sha256(bytes).take(16)}-c$CONVERTER"
        val jar = File(cacheDir, "$pkg-$stamp.jar")
        val manifest = File(cacheDir, "$pkg-$stamp.meta")
        if (!jar.isFile || !manifest.isFile) {
            verify(apk, signer)
            convert(bytes, jar)
            manifest.writeText(metadata(apk).entries.joinToString("\n") { "${it.key}=${it.value}" })
            // Older conversions of this extension are no longer needed.
            cacheDir.listFiles { f -> f.name.startsWith("$pkg-") && !f.name.startsWith("$pkg-$stamp") }?.forEach { it.delete() }
        }
        val meta = manifest.readLines().associate { it.substringBefore('=') to it.substringAfter('=') }
        val lib = meta["tachiyomix.extensionLib"]?.takeIf { it.isNotEmpty() && it != "0.0" } ?: meta["versionName"].orEmpty().substringBeforeLast('.')
        if (lib.toDoubleOrNull() !in libs.map { it.toDouble() }) {
            throw HostError("This extension uses lib $lib, which this SCANCORE does not run.", 426)
        }
        val classes =
            meta["tachiyomi.extension.class"]?.split(";")?.map { it.trim() }?.filter { it.isNotEmpty() }
                ?: throw HostError("The extension does not say which sources it has.", 422)
        val loader = ChildFirstClassLoader(arrayOf(jar.toURI().toURL()), javaClass.classLoader)
        return classes.flatMap { name ->
            val className = if (name.startsWith(".")) pkg + name else name
            when (val instance = Class.forName(className, false, loader).getDeclaredConstructor().newInstance()) {
                is Source -> listOf(instance)
                is SourceFactory -> instance.createSources()
                else -> throw HostError("The extension's class $className is not a source.", 422)
            }
        }
    }

    private fun verify(
        apk: File,
        signer: String?,
    ) {
        val result = ApkVerifier.Builder(apk).build().verify()
        if (!result.isVerified) {
            val reason = result.errors.firstOrNull()?.toString() ?: "unknown"
            throw HostError("The extension's signature does not check out ($reason).", 403)
        }
        if (signer != null) {
            val signers = result.signerCertificates.map { sha256(it.encoded) }
            if (signer.lowercase() !in signers) throw HostError("The extension is signed with a key SCANCORE did not trust.", 403)
        }
    }

    private fun convert(
        apk: ByteArray,
        jar: File,
    ) {
        cacheDir.mkdirs()
        val temp = File(jar.path + ".part")
        temp.delete()
        val handler = BaksmaliBaseDexExceptionHandler()
        Dex2jar
            .from(MultiDexFileReader.open(apk))
            .withExceptionHandler(handler)
            .reUseReg(false)
            .topoLogicalSort()
            .skipDebug(true)
            .optimizeSynchronized(false)
            .printIR(false)
            .noCode(false)
            .skipExceptions(false)
            .dontSanitizeNames(true)
            .computeFrames(true)
            .to(temp.toPath())
        if (handler.hasException()) {
            temp.delete()
            throw HostError("The extension could not be converted to run on this computer.", 422)
        }
        BytecodeEditor.fixAndroidClasses(temp.toPath())
        if (!temp.renameTo(jar)) throw HostError("The converted extension could not be saved.", 500)
    }

    // The meta-data of the manifest's <application>, and the version name.
    private fun metadata(apk: File): Map<String, String> =
        ApkFile(apk).use { parsed ->
            val doc = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(parsed.manifestXml.byteInputStream())
            val out = mutableMapOf("versionName" to (parsed.apkMeta.versionName ?: ""))
            val nodes = doc.getElementsByTagName("meta-data")
            for (i in 0 until nodes.length) {
                val e = nodes.item(i) as? Element ?: continue
                out[e.getAttribute("android:name")] = e.getAttribute("android:value").replace('\n', ' ')
            }
            out
        }

    private fun sha256(bytes: ByteArray) = MessageDigest.getInstance("SHA-256").digest(bytes).joinToString("") { "%02x".format(it) }
}

// Classes come from the extension first, so its own copies of libraries win;
// the host's API classes are not in the extension, so they come from the
// host, which is what makes it run.
class ChildFirstClassLoader(
    urls: Array<java.net.URL>,
    parent: ClassLoader,
) : URLClassLoader(urls, parent) {
    override fun loadClass(
        name: String,
        resolve: Boolean,
    ): Class<*> =
        synchronized(getClassLoadingLock(name)) {
            val c =
                findLoadedClass(name)
                    ?: if (name.startsWith("java.") || name.startsWith("javax.") || name.startsWith("kotlin.")) {
                        super.loadClass(name, false)
                    } else {
                        try {
                            findClass(name)
                        } catch (_: ClassNotFoundException) {
                            super.loadClass(name, false)
                        }
                    }
            if (resolve) resolveClass(c)
            c
        }
}
