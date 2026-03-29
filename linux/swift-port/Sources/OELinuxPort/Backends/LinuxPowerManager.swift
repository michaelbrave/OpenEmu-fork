import Foundation

public final class LinuxPowerManager {
    private(set) public var isInhibited = false

    public init() {}

    public func inhibit() {
        isInhibited = true
    }

    public func uninhibit() {
        isInhibited = false
    }
}
