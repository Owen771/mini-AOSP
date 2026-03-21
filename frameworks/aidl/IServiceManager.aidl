// mini-AOSP IServiceManager — AIDL interface definition (placeholder for Stage 0)
// Real codegen produces Kotlin/C++ stubs in Stage 2

interface IServiceManager {
    // Register a service by name and socket path
    void addService(String name, String socketPath);

    // Look up a service by name, returns socket path or null
    String getService(String name);

    // List all registered service names
    String[] listServices();
}
