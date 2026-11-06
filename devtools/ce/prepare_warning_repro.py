"""Generate isolated compile-only projects using baseline source blobs and real Release flags."""
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
BUILD=ROOT/'build-scummvm'
sources=BUILD/'acceptance-warning-sources'
sources.mkdir(exist_ok=True)
ns={'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
ET.register_namespace('',ns['m'])
for project, revision, paths in [
    ('sci','29e2ceb01611ac18654d82bfe2116db8183bd53d',['engines/sci/dialogs.cpp','engines/sci/graphics/screen.cpp']),
    ('fool','29e2ceb01611ac18654d82bfe2116db8183bd53d',['engines/fool/fool_game.cpp']),
    ('phoenixvr','7d2709da5136866589f7602f5f5a7b17de03420e',['engines/phoenixvr/phoenixvr.cpp'])]:
    tree=ET.parse(BUILD/f'{project}.vcxproj')
    root=tree.getroot()
    for parent in root.iter():
        for element in list(parent):
            if element.tag in [f"{{{ns['m']}}}ClCompile",f"{{{ns['m']}}}ProjectReference"] and 'Include' in element.attrib:
                parent.remove(element)
    group=ET.SubElement(root,f"{{{ns['m']}}}ItemGroup")
    for path in paths:
        content=subprocess.check_output(['git','-c',f'safe.directory={ROOT.as_posix()}','-C',str(ROOT),'show',f'{revision}:{path}'])
        target=sources/f'{project}-{Path(path).name}'
        target.write_bytes(content)
        compile=ET.SubElement(group,f"{{{ns['m']}}}ClCompile",Include=str(target.relative_to(BUILD)))
        ET.SubElement(compile,f"{{{ns['m']}}}ObjectFileName").text='$(IntDir)%(Filename).obj'
        if project == 'fool':
            ET.SubElement(compile,f"{{{ns['m']}}}AdditionalIncludeDirectories").text=str(ROOT/Path(path).parent)+';%(AdditionalIncludeDirectories)'
    tree.write(BUILD/f'acceptance-warning-{project}.vcxproj',encoding='utf-8',xml_declaration=True)
    print(project,revision,','.join(paths))
