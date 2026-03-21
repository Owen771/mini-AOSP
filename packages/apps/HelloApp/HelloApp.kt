package apps

import os.Log
import java.net.StandardProtocolFamily
import java.net.UnixDomainSocketAddress
import java.nio.channels.SocketChannel
import java.nio.ByteBuffer

/**
 * mini-AOSP HelloApp — connects to servicemanager, resolves PingService,
 * sends PING, receives PONG, logs round-trip time.
 */
object HelloApp {
    private const val TAG = "HelloApp"
    private const val SM_SOCKET = "/tmp/mini-aosp/servicemanager.sock"

    @JvmStatic
    fun main(args: Array<String>) {
        Log.i(TAG, "Connecting to servicemanager...")

        // Resolve PingService via servicemanager
        val pingSocket = resolveService("ping")
        if (pingSocket == null) {
            Log.e(TAG, "Could not resolve service: ping")
            System.exit(1)
        }
        Log.i(TAG, "Resolved service: ping → $pingSocket")

        // Send PING to PingService
        Log.i(TAG, "Sending PING...")
        val startTime = System.nanoTime()

        val response = sendPing(pingSocket!!)
        val elapsed = (System.nanoTime() - startTime) / 1_000_000.0

        if (response != null && response.startsWith("PONG")) {
            Log.i(TAG, "Received: PONG — round-trip %.1fms".format(elapsed))
            Log.i(TAG, "✓ Full stack verified: App → SystemServer → ServiceManager → init → kernel")
        } else {
            Log.e(TAG, "Unexpected response: $response")
            System.exit(1)
        }
    }

    private fun resolveService(name: String): String? {
        return try {
            val smAddr = UnixDomainSocketAddress.of(SM_SOCKET)
            val channel = SocketChannel.open(StandardProtocolFamily.UNIX)
            channel.connect(smAddr)

            val request = "GET_SERVICE $name\n"
            channel.write(ByteBuffer.wrap(request.toByteArray()))

            val buf = ByteBuffer.allocate(1024)
            channel.read(buf)
            buf.flip()
            val response = String(buf.array(), 0, buf.limit()).trim()
            channel.close()

            if (response == "NOT_FOUND") null else response
        } catch (e: Exception) {
            Log.e(TAG, "Failed to resolve service: ${e.message}")
            null
        }
    }

    private fun sendPing(socketPath: String): String? {
        return try {
            val addr = UnixDomainSocketAddress.of(socketPath)
            val channel = SocketChannel.open(StandardProtocolFamily.UNIX)
            channel.connect(addr)

            channel.write(ByteBuffer.wrap("PING\n".toByteArray()))

            val buf = ByteBuffer.allocate(1024)
            channel.read(buf)
            buf.flip()
            val response = String(buf.array(), 0, buf.limit()).trim()
            channel.close()

            response
        } catch (e: Exception) {
            Log.e(TAG, "Failed to send PING: ${e.message}")
            null
        }
    }
}
