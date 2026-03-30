import Foundation
import OELinuxPort

@main
struct OELinuxHeadlessCLI {
    static func main() throws {
        let args = CommandLine.arguments
        guard args.count >= 3 else {
            fputs("usage: oe-headless <core_path> <rom_path>\n", stderr)
            return
        }

        let corePath = args[1]
        let romPath = args[2]

        print("Loading core: \(corePath)")
        let core = try CoreBridge(libraryPath: corePath)

        print("Loading ROM: \(romPath)")
        try core.loadROM(path: romPath)

        print("Running 60 frames...")
        for i in 1...60 {
            core.runFrame()
            if i % 10 == 0 {
                print("  frame \(i)/60")
            }
        }

        let size = core.videoSize
        print("Final frame size: \(size.width)x\(size.height)")
        
        let sampleRate = core.audioSampleRate
        print("Audio sample rate: \(sampleRate)")
        
        var audioBuffer = Array<Int16>(repeating: 0, count: 4096)
        let samplesRead = core.readAudio(into: &audioBuffer, maxSamples: 4096)
        print("Read \(samplesRead) audio samples from buffer")
    }
}
