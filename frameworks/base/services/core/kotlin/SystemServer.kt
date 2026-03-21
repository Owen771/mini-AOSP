package services

import os.Log
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.PrintWriter
import java.net.StandardProtocolFamily
import java.net.UnixDomainSocketAddress
import java.nio.channels.ServerSocketChannel
import java.nio.channels.SocketChannel
import java.nio.ByteBuffer
import java.nio.file.Path

/**
 * mini-AOSP SystemServer — registers PingService with servicemanager,
 * then listens for PING requests and responds with PONG + caller info.
 */
object SystemServer {
    private const val TAG = "system_server"
    private const val SM_SOCKET = "/tmp/mini-aosp/servicemanager.sock"
    private const val PING_SOCKET = "/tmp/mini-aosp/ping.sock"
    private const val SERVICE_NAME = "ping"

    @Volatile
    private var running = true

    @JvmStatic
    fun main(args: Array<String>) {
        Runtime.getRuntime().addShutdownHook(Thread { running = false })

        // Connect to servicemanager and register our service
        Log.i(TAG, "Connecting to servicemanager...")
        val registered = registerService(SERVICE_NAME, PING_SOCKET)
        if (!registered) {
            Log.e(TAG, "Failed to register service with servicemanager")
            System.exit(1)
        }
        Log.i(TAG, "Registered service: $SERVICE_NAME → $PING_SOCKET")

        // Start PingService listener
        startPingService()
    }

    private fun registerService(name: String, socketPath: String): Boolean {
        return try {
            val smAddr = UnixDomainSocketAddress.of(SM_SOCKET)
            val channel = SocketChannel.open(StandardProtocolFamily.UNIX)
            channel.connect(smAddr)

            val request = "ADD_SERVICE $name $socketPath\n"
            channel.write(ByteBuffer.wrap(request.toByteArray()))

            val buf = ByteBuffer.allocate(1024)
            channel.read(buf)
            buf.flip()
            val response = String(buf.array(), 0, buf.limit()).trim()
            channel.close()

            response == "OK"
        } catch (e: Exception) {
            Log.e(TAG, "Registration failed: ${e.message}")
            false
        }
    }

    private fun startPingService() {
        // Clean up stale socket
        val socketFile = java.io.File(PING_SOCKET)
        if (socketFile.exists()) socketFile.delete()

        val addr = UnixDomainSocketAddress.of(PING_SOCKET)
        val server = ServerSocketChannel.open(StandardProtocolFamily.UNIX)
        server.bind(addr)

        Log.i(TAG, "PingService listening...")

        while (running) {
            try {
                server.configureBlocking(false)
                val client = server.accept()
                if (client == null) {
                    Thread.sleep(100)
                    continue
                }

                val buf = ByteBuffer.allocate(1024)
                client.read(buf)
                buf.flip()
                val request = String(buf.array(), 0, buf.limit()).trim()

                if (request == "PING") {
                    val uid = ProcessHandle.current().pid() // approximate; real uid from SO_PEERCRED
                    val pid = ProcessHandle.current().pid()
                    val time = System.currentTimeMillis()
                    Log.i(TAG, "PingService: PING from uid=$uid pid=$pid")

                    val response = "PONG uid=$uid pid=$pid time=${time}\n"
                    client.write(ByteBuffer.wrap(response.toByteArray()))
                }

                client.close()
            } catch (e: Exception) {
                if (running) {
                    Thread.sleep(100)
                }
            }
        }

        server.close()
        socketFile.delete()
        Log.i(TAG, "PingService stopped.")
    }
}
