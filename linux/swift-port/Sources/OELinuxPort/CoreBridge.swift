import Foundation

public enum PixelFormat: Int32 {
    case rgba8888 = 0
    case rgb565 = 1
    case indexed8 = 2
}

public final class CoreBridge {
    private typealias GetInterfaceFn = @convention(c) () -> UnsafePointer<OECoreInterface>?
    
    private let handle: UnsafeMutableRawPointer
    private let interface: OECoreInterface
    private var state: OpaquePointer?

    public init(libraryPath: String) throws {
        guard let handle = dlopen(libraryPath, RTLD_LAZY) else {
            let error = String(cString: dlerror())
            throw NSError(domain: "CoreBridge", code: 1, userInfo: [NSLocalizedDescriptionKey: "dlopen failed: \(error)"])
        }
        self.handle = handle

        guard let symbol = dlsym(handle, "oe_get_core_interface") else {
            dlclose(handle)
            throw NSError(domain: "CoreBridge", code: 2, userInfo: [NSLocalizedDescriptionKey: "dlsym failed to find oe_get_core_interface"])
        }

        let getInterface = unsafeBitCast(symbol, to: GetInterfaceFn.self)
        guard let interfacePtr = getInterface() else {
            dlclose(handle)
            throw NSError(domain: "CoreBridge", code: 3, userInfo: [NSLocalizedDescriptionKey: "oe_get_core_interface returned NULL"])
        }
        self.interface = interfacePtr.pointee
        
        guard let state = self.interface.create() else {
            dlclose(handle)
            throw NSError(domain: "CoreBridge", code: 4, userInfo: [NSLocalizedDescriptionKey: "core create failed"])
        }
        self.state = state
    }

    deinit {
        if let state = state {
            interface.destroy(state)
        }
        dlclose(handle)
    }

    public func loadROM(path: String) throws {
        guard let state = state else { return }
        let result = interface.load_rom(state, path)
        if result != 0 {
            throw NSError(domain: "CoreBridge", code: 5, userInfo: [NSLocalizedDescriptionKey: "load_rom failed with code \(result)"])
        }
    }

    public func runFrame() {
        guard let state = state else { return }
        interface.run_frame(state)
    }

    public func reset() {
        guard let state = state else { return }
        interface.reset(state)
    }

    public func saveState(path: String) throws {
        guard let state = state else { return }
        let result = interface.save_state(state, path)
        if result != 0 {
            throw NSError(domain: "CoreBridge", code: 6, userInfo: [NSLocalizedDescriptionKey: "save_state failed with code \(result)"])
        }
    }

    public func loadState(path: String) throws {
        guard let state = state else { return }
        let result = interface.load_state(state, path)
        if result != 0 {
            throw NSError(domain: "CoreBridge", code: 7, userInfo: [NSLocalizedDescriptionKey: "load_state failed with code \(result)"])
        }
    }

    public func setButton(_ button: Int, player: Int, pressed: Bool) {
        guard let state = state else { return }
        interface.set_button(state, Int32(player), Int32(button), pressed ? 1 : 0)
    }

    public var videoSize: (width: Int, height: Int) {
        var w: Int32 = 0
        var h: Int32 = 0
        if let state = state {
            interface.get_video_size(state, &w, &h)
        }
        return (Int(w), Int(h))
    }

    public var pixelFormat: PixelFormat {
        if let state = state {
            return PixelFormat(rawValue: interface.get_pixel_format(state).rawValue) ?? .rgba8888
        }
        return .rgba8888
    }

    public var videoBuffer: UnsafeRawPointer? {
        guard let state = state else { return nil }
        return interface.get_video_buffer(state)
    }

    public var audioSampleRate: Int {
        guard let state = state else { return 0 }
        return Int(interface.get_audio_sample_rate(state))
    }

    public func readAudio(into buffer: UnsafeMutablePointer<Int16>, maxSamples: Int) -> Int {
        guard let state = state else { return 0 }
        return interface.read_audio(state, buffer, maxSamples)
    }
}

// C-compatible struct matching oe_core_interface.h
// Note: This needs to be defined for the Swift compiler to know the layout.
// In a real project, we might use a module map to import the C header.
struct OECoreInterface {
    var create: @convention(c) () -> OpaquePointer?
    var destroy: @convention(c) (OpaquePointer?) -> Void
    var load_rom: @convention(c) (OpaquePointer?, UnsafePointer<CChar>?) -> Int32
    var run_frame: @convention(c) (OpaquePointer?) -> Void
    var reset: @convention(c) (OpaquePointer?) -> Void
    var save_state: @convention(c) (OpaquePointer?, UnsafePointer<CChar>?) -> Int32
    var load_state: @convention(c) (OpaquePointer?, UnsafePointer<CChar>?) -> Int32
    var set_button: @convention(c) (OpaquePointer?, Int32, Int32, Int32) -> Void
    var set_axis: @convention(c) (OpaquePointer?, Int32, Int32, Int32) -> Void
    var get_video_size: @convention(c) (OpaquePointer?, UnsafeMutablePointer<Int32>?, UnsafeMutablePointer<Int32>?) -> Void
    var get_pixel_format: @convention(c) (OpaquePointer?) -> OEPixelFormat
    var get_video_buffer: @convention(c) (OpaquePointer?) -> UnsafeRawPointer?
    var get_audio_sample_rate: @convention(c) (OpaquePointer?) -> Int32
    var read_audio: @convention(c) (OpaquePointer?, UnsafeMutablePointer<Int16>?, Int) -> Int
}

enum OEPixelFormat: Int32 {
    case rgba8888 = 0
    case rgb565 = 1
    case indexed8 = 2
}
