import Foundation
import PDFKit
import AppKit
let root=URL(fileURLWithPath:"/Users/binova/Downloads/T SHEETS FOR FACULTY ONLY")
let out=URL(fileURLWithPath:"/tmp/datumforge-course-review")
let selections:[(String,[Int])]=[("T2 PART IN 2D.pdf",[4,17,18,35]),("T3 EDITING PART.pdf",[24,32]),("T4 ADD RELATIONS.pdf",[12]),("T5 ADD PART FEATURES.pdf",[24]),("T6 REFERENCE GEOMETRY.pdf",[33]),("T7 MODELLING TOOLS.pdf",[21]),("T8 ADVANCE MODELLING TOOLS.pdf",[35]),("T9 ASSEMBLY PART 1 .pdf",[20]),("T9 ASSEMBLY PART 2.pdf",[7]),("T10 DRAWING.pdf",[26,35])]
for (name,numbers) in selections {
 let doc=PDFDocument(url:root.appendingPathComponent(name))!
 for n in numbers {
  let p=doc.page(at:n-1)!, image=p.thumbnail(of:NSSize(width:1100,height:1500),for:.mediaBox)
  let rep=NSBitmapImageRep(data:image.tiffRepresentation!)!
  try rep.representation(using:.png,properties:[:])!.write(to:out.appendingPathComponent(name+"-p\(n).png"))
 }
}
print("Rendered 15 selected pages with native PDFKit")
