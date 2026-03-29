import Foundation

public enum OESystemInputEvent {
    case press(String)
    case release(String)
    case axis(String, Float)
}

public protocol InputBackend {
    func start() throws
    func stop()
    func pollEvents() -> [OESystemInputEvent]
}
