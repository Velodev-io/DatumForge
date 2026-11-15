// Native macOS PDFKit validation/rendering; no installation or network dependency.
import Foundation
import PDFKit
import AppKit
let args = CommandLine.arguments
if args.count != 3 { fatalError("usage: inspect_drawing_pdf.swift PDF OUTPUT_DIRECTORY") }
let url = URL(fileURLWithPath: args[1]), folder = URL(fileURLWithPath: args[2], isDirectory: true)
try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
guard let doc = PDFDocument(url: url) else { fatalError("Unreadable PDF") }
var pages: [[String: Any]] = []
for index in 0..<doc.pageCount {
    guard let page = doc.page(at: index) else { fatalError("Missing page") }
    let bounds = page.bounds(for: .mediaBox)
    guard abs(bounds.width - 297 * 72 / 25.4) < 0.5 && abs(bounds.height - 210 * 72 / 25.4) < 0.5 else { fatalError("Incorrect A4 landscape page geometry: \(bounds)") }
    let scale: CGFloat = 1.5
    let width = Int(bounds.width * scale), height = Int(bounds.height * scale)
    guard let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: width, pixelsHigh: height, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0), let graphics = NSGraphicsContext(bitmapImageRep: bitmap) else { fatalError("Cannot allocate bounded preview") }
    NSGraphicsContext.saveGraphicsState(); NSGraphicsContext.current = graphics
    graphics.cgContext.setFillColor(NSColor.white.cgColor); graphics.cgContext.fill(CGRect(x: 0, y: 0, width: width, height: height)); graphics.cgContext.scaleBy(x: scale, y: scale)
    page.draw(with: .mediaBox, to: graphics.cgContext)
    NSGraphicsContext.restoreGraphicsState()
    guard let png = bitmap.representation(using: .png, properties: [:]) else { fatalError("PNG failure") }
    try png.write(to: folder.appendingPathComponent("page-\(index+1).png"))
    let text = page.string ?? ""
    guard !text.isEmpty else { fatalError("PDF lacks extractable vector text") }
    pages.append(["page": index + 1, "page_rounding_tolerance_points": 0.5, "width_points": bounds.width, "height_points": bounds.height, "text": text])
}
let data = try Data(contentsOf: url)
let raw = String(decoding: data, as: UTF8.self)
guard !raw.contains("/Subtype /Image") && !raw.contains("/Subtype/Image") else { fatalError("Unexpected raster image in vector-only PDF") }
guard doc.pageCount == 2, (pages[0]["text"] as? String ?? "").contains("80.000 mm"), (pages[1]["text"] as? String ?? "").contains("BOM") else { fatalError("B5 page/text contract failed") }
let result: [String: Any] = ["status": "Pass", "renderer": "macOS PDFKit", "raster_images": 0, "pages": pages, "limits": "Page geometry, extractable text, raster absence and native rendering; visual review recorded separately. Geometric scale is asserted against actual production vector path coordinates in C++."]
try JSONSerialization.data(withJSONObject: result, options: [.prettyPrinted,.sortedKeys]).write(to: folder.appendingPathComponent("pdf-inspection.json"))
print("Pass: two A4 landscape pages, embedded/extractable dimension and BOM text, no raster images; PNGs rendered")
