import Foundation

public protocol AudioBackend {
    func start(sampleRate: Int, channels: Int) throws
    func stop()
    func enqueue(samples: UnsafeBufferPointer<Int16>)
}
