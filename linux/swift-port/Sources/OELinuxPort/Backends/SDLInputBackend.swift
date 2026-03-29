import Foundation

public final class SDLInputBackend: InputBackend {
    private var pendingEvents: [OESystemInputEvent] = []

    public init() {}

    public func start() throws {
        // SDL event pump hookup lands here in the next pass.
    }

    public func stop() {
        pendingEvents.removeAll(keepingCapacity: false)
    }

    public func pollEvents() -> [OESystemInputEvent] {
        defer { pendingEvents.removeAll(keepingCapacity: true) }
        return pendingEvents
    }

    public func inject(_ event: OESystemInputEvent) {
        pendingEvents.append(event)
    }
}
