"""Read the frozen NASA PDFs; leave the source files unchanged."""
from pathlib import Path
import json
from pypdf import PdfReader

ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'sources/extracted';out.mkdir(exist_ok=True)
for pdf in sorted((ROOT/'sources').glob('*.pdf')):
    reader=PdfReader(pdf)
    pages=[{'pdf_page':i+1,'text':page.extract_text() or ''} for i,page in enumerate(reader.pages)]
    stem='mission_report' if 'Mission_Report' in pdf.name else 'trajectory_supplement'
    (out/(stem+'.json')).write_text(json.dumps(pages,indent=2),encoding='utf-8')
    (out/(stem+'.txt')).write_text('\n\n'.join(f"PDF PAGE {p['pdf_page']}\n{p['text']}" for p in pages),encoding='utf-8')
    matches=[{'pdf_page':p['pdf_page'],'excerpt':p['text'][:130]} for p in pages if any(s in p['text'].lower() for s in ['24.8','besselian','165,561','165 561','3.4 entry','boeing','16379409','1.6379409','302856684'])]
    print(json.dumps({'source':pdf.name,'pages':len(pages),'selected_matches':matches},indent=2),flush=True)
