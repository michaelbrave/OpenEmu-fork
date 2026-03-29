import Foundation

public final class SDLAudioBackend: AudioBackend {
    private var ringBuffer: [Int16] = []

    public init() {}

    public func start(sampleRate: Int, channels: Int) throws {
        ringBuffer.removeAll(keepingCapacity: false)
        ringBuffer.reserveCapacity(sampleRate * channels)
    }

    public func stop() {
        ringBuffer.removeAll(keepingCapacity: false)
    }

    public func enqueue(samples: UnsafeBufferPointer<Int16>) {
        ringBuffer.append(contentsOf: samples)
    }

    public func drain(maxSamples: Int) -> [Int16] {
        let count = min(maxSamples, ringBuffer.count)
        let head = Array(ringBuffer.prefix(count))
        ringBuffer.removeFirst(count)
        return head
    }
}
