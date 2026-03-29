// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "OELinuxPort",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .library(name: "OELinuxPort", targets: ["OELinuxPort"]),
        .executable(name: "oe-headless", targets: ["oe-headless"])
    ],
    dependencies: [
        .package(url: "https://github.com/groue/GRDB.swift", from: "6.29.0")
    ],
    targets: [
        .target(
            name: "OELinuxPort",
            dependencies: [
                .product(name: "GRDB", package: "GRDB.swift")
            ]
        ),
        .executableTarget(
            name: "oe-headless",
            dependencies: ["OELinuxPort"]
        )
    ]
)
