import Foundation
import PDFKit
import AppKit
let source = URL(fileURLWithPath: "/Users/binova/Downloads/T SHEETS FOR FACULTY ONLY")
let dest = URL(fileURLWithPath: "/tmp/datumforge-course-review")
var manifest: [[String: Any]] = []
for file in try FileManager.default.contentsOfDirectory(at: source, includingPropertiesForKeys: nil).filter({$0.pathExtension.lowercased() == "pdf"}).sorted(by: {$0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedAscending}) {
 guard let doc = PDFDocument(url: file) else { fatalError("Cannot read \(file)") }
 var pages: [[String:Any]] = []; var full = ""
 for i in 0..<doc.pageCount {
  let page = doc.page(at:i)!; let txt = page.string ?? ""
  pages.append(["page":i+1, "text":txt]); full += "\n=== PDF PAGE \(i+1) ===\n" + txt + "\n"
 }
 let stem = file.deletingPathExtension().lastPathComponent
 try JSONSerialization.data(withJSONObject: pages, options: [.prettyPrinted,.sortedKeys]).write(to:dest.appendingPathComponent(stem+".json"))
 try full.write(to:dest.appendingPathComponent(stem+".txt"), atomically:true, encoding:.utf8)
 manifest.append(["file":file.lastPathComponent,"pages":doc.pageCount,"characters":full.count,"empty_pages":pages.filter{($0["text"] as! String).trimmingCharacters(in:.whitespacesAndNewlines).isEmpty}.count])
 // Render a small complete contact sheet for each PDF; page-specific rendering follows.
 let columns=5, cellW=210, cellH=310, rows=(doc.pageCount+4)/5
 let bitmap=NSBitmapImageRep(bitmapDataPlanes:nil,pixelsWide:columns*cellW,pixelsHigh:rows*cellH,bitsPerSample:8,samplesPerPixel:4,hasAlpha:true,isPlanar:false,colorSpaceName:.deviceRGB,bytesPerRow:0,bitsPerPixel:0)!
 let gc=NSGraphicsContext(bitmapImageRep:bitmap)!
 NSGraphicsContext.saveGraphicsState(); NSGraphicsContext.current=gc
 gc.cgContext.setFillColor(NSColor.white.cgColor);gc.cgContext.fill(CGRect(x:0,y:0,width:columns*cellW,height:rows*cellH))
 for i in 0..<doc.pageCount {
  let p=doc.page(at:i)!, bounds=p.bounds(for:.mediaBox)
  let x=(i%columns)*cellW, y=(rows-1-i/columns)*cellH
  gc.cgContext.saveGState();gc.cgContext.translateBy(x:CGFloat(x+5),y:CGFloat(y+25))
  let scale=min(CGFloat(cellW-10)/bounds.width,CGFloat(cellH-35)/bounds.height)
  gc.cgContext.scaleBy(x:scale,y:scale);p.draw(with:.mediaBox,to:gc.cgContext);gc.cgContext.restoreGState()
  ("Page \(i+1)" as NSString).draw(at: NSPoint(x:x+8,y:y+5),withAttributes:[.font:NSFont.systemFont(ofSize:12),.foregroundColor:NSColor.black])
 }
 NSGraphicsContext.restoreGraphicsState()
 try bitmap.representation(using:.png,properties:[:])!.write(to:dest.appendingPathComponent(stem+"-contact.png"))
}
try JSONSerialization.data(withJSONObject:manifest,options:[.prettyPrinted,.sortedKeys]).write(to:dest.appendingPathComponent("manifest.json"))
print(String(data:try JSONSerialization.data(withJSONObject:manifest,options:.prettyPrinted),encoding:.utf8)!)
