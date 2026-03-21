package os

/**
 * mini-AOSP Log utility — matches the C++ log format for consistent output.
 * Usage: Log.i("HelloApp", "Sending PING...")
 */
object Log {
    fun i(tag: String, message: String) {
        val padded = tag.padEnd(16)
        println("[$padded] $message")
    }

    fun e(tag: String, message: String) {
        val padded = tag.padEnd(16)
        System.err.println("[$padded] ERROR: $message")
    }

    fun d(tag: String, message: String) {
        val padded = tag.padEnd(16)
        println("[$padded] $message")
    }
}
