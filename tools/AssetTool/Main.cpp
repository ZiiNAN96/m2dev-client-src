#include "Scene.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iostream>
#include <locale>
#include <map>
#include <set>

namespace Tool=ZiiNAN::AssetTool;
namespace fs=std::filesystem;
namespace {
constexpr const char* Help=R"(ZiiNAN offline asset tool
  ziinan-asset-tool convert <input> <output.glb> [options]
  ziinan-asset-tool validate <file> [options]
  ziinan-asset-tool inspect <file> [options]
  ziinan-asset-tool batch <input-dir> <output-dir> [options]
  ziinan-asset-tool --help | --version
Sources: FBX, OBJ, DAE, PLY, STL. GLB inspection uses the E1-X provider.
Options:
  --json                    Structured diagnostics and statistics
  --no-optimize             Disable static cache/fetch optimization
  --generate-normals        Generate only missing normals (default)
  --no-generate-normals     Disable offline normal generation
  --generate-tangents       Generate tangents when UV0 is available
  --meters-per-unit <value> Explicit source unit override
  --ticks-per-second <rate> Explicit animation timebase override
Output must not exist. Batch preserves folders and rejects name collisions.
Exit codes: 0 success, 2 usage, 3 invalid input/asset, 4 output failure.
Skins and animations are exported as data; E1-X does not animate GLB skins.
)";
std::string Lower(std::string value)
{
    for(auto& c:value) if(c>='A' && c<='Z') c=char(c+'a'-'A'); return value;
}
void Print(const Tool::Scene& scene,const Tool::Report& report,bool json)
{
    if(json) { std::cout << Tool::ToJSON(scene,report) << '\n'; return; }
    std::size_t vertices=0,indices=0,influences=0;
    for(const auto& m:scene.meshes) { vertices+=m.vertices.size(); indices+=m.indices.size(); for(const auto& v:m.vertices) influences=std::max(influences,v.influences.size()); }
    std::cout << "Asset: " << scene.name << "\nType: " << (!scene.skeleton.joints.empty()?"SKINNED":scene.animations.empty()?"STATIC":"ANIMATED_RIGID")
        << "\nMeshes: " << scene.meshes.size() << "\nVertices: " << vertices << "\nIndices: " << indices
        << "\nMaterials: " << scene.materials.size() << "\nTextures: " << scene.images.size()
        << "\nBones: " << scene.skeleton.joints.size() << "\nAnimations: " << scene.animations.size() << "\nInfluence maximum: " << influences
        << "\nUnits: " << scene.unitPolicy << '\n';
    if(scene.bounds.valid) std::cout << "Bounds (meters): [" << scene.bounds.min[0] << ',' << scene.bounds.min[1] << ',' << scene.bounds.min[2]
        << "] to [" << scene.bounds.max[0] << ',' << scene.bounds.max[1] << ',' << scene.bounds.max[2] << "]\n";
    std::size_t errors=0;
    for(const auto& i:report.issues) { errors+=i.error; std::cout << (i.error?"ERROR ":"WARNING ") << i.code << " [" << i.context << "]: " << i.message << '\n'; }
    std::cout << "Errors: " << errors << "\nWarnings: " << report.issues.size()-errors << '\n';
}
bool Positive(const std::string& arg,double& value)
{
    const auto [last,ec]=std::from_chars(arg.data(),arg.data()+arg.size(),value);
    return ec==std::errc{} && last==arg.data()+arg.size() && std::isfinite(value) && value>0;
}
int Convert(const fs::path& input,const fs::path& output,const Tool::Options& options,bool json)
{
    Tool::Report report; Tool::Scene scene; scene.name=Tool::PathUTF8(input.filename());
    if(Lower(Tool::PathUTF8(output.extension()))!=".glb") { report.Error("output",Tool::PathUTF8(output),"Output extension must be .glb."); Print(scene,report,json); return 2; }
    if(fs::exists(output)) { report.Error("output",Tool::PathUTF8(output),"Output already exists; choose a fresh path."); Print(scene,report,json); return 4; }
    if(!Tool::Import(input,options,scene,report) || !Tool::Process(scene,options,report)) { Print(scene,report,json); return 3; }
    if(!Tool::WriteGLB(scene,output,report)) {
        bool outputError=false; for(const auto& issue:report.issues) outputError|=issue.code=="output";
        Print(scene,report,json); return outputError?4:3;
    }
    Print(scene,report,json); return 0;
}
int Batch(const fs::path& input,const fs::path& output,const Tool::Options& options,bool json)
{
    if(!fs::is_directory(input)) throw std::runtime_error("Batch input is not a readable directory");
    auto source=fs::canonical(input), destination=fs::absolute(output).lexically_normal();
    auto ancestor=destination; fs::path suffix;
    while(!fs::exists(ancestor)) {
        suffix=ancestor.filename()/suffix;
        const auto parent=ancestor.parent_path();
        if(parent==ancestor || parent.empty()) throw std::runtime_error("Batch output has no accessible parent");
        ancestor=parent;
    }
    destination=(fs::canonical(ancestor)/suffix).lexically_normal();
    const auto relative=destination.lexically_relative(source);
    if(!relative.empty() && *relative.begin()!=fs::path("..")) throw std::runtime_error("Batch output must be outside its input directory");
    std::vector<std::pair<fs::path,fs::path>> files; std::set<std::string> outputs;
    for(const auto& entry:fs::recursive_directory_iterator(source)) {
        if(entry.is_symlink() || !entry.is_regular_file() || !Tool::IsSupportedSource(entry.path())) continue;
        auto target=destination/entry.path().lexically_relative(source); target.replace_extension(".glb");
        if(!outputs.insert(Lower(Tool::PathUTF8(target))).second || fs::exists(target))
            throw std::runtime_error("Batch output collision: "+Tool::PathUTF8(target));
        files.emplace_back(entry.path(),target);
    }
    std::sort(files.begin(),files.end());
    if(files.empty()) throw std::runtime_error("No supported source files in batch directory");
    int result=0;
    for(const auto& [from,to]:files) {
        std::error_code error; fs::create_directories(to.parent_path(),error);
        if(error) { Tool::Scene scene; scene.name=Tool::PathUTF8(from); Tool::Report report; report.Error("output",Tool::PathUTF8(to),error.message()); Print(scene,report,json); result=4; continue; }
        result=std::max(result,Convert(from,to,options,json));
    }
    return result;
}
int Run(const std::vector<std::string>& args)
{
    if(args.empty()) { std::cout << Help; return 2; }
    if(args.size()==1 && args[0]=="--help") { std::cout << Help; return 0; }
    if(args.size()==1 && args[0]=="--version") { std::cout << "ZiiNAN Asset Tool " << Tool::Version << " | Assimp 6.0.2 | meshoptimizer 0.25 | cgltf 1.15\n"; return 0; }
    const auto& command=args[0]; const bool two=command=="convert" || command=="batch";
    if(!two && command!="validate" && command!="inspect") { std::cerr << "Unknown command. Use --help.\n"; return 2; }
    if(args.size()<(two?3u:2u)) { std::cerr << "Missing path. Use --help.\n"; return 2; }
    Tool::Options options; bool json=false;
    for(std::size_t i=two?3:2;i<args.size();++i) {
        const auto& arg=args[i];
        if(arg=="--json") json=true;
        else if(arg=="--no-optimize") options.optimize=false;
        else if(arg=="--generate-normals") options.generateNormals=true;
        else if(arg=="--no-generate-normals") options.generateNormals=false;
        else if(arg=="--generate-tangents") options.generateTangents=true;
        else if(arg=="--meters-per-unit" || arg=="--ticks-per-second") {
            double value{}; if(++i>=args.size() || !Positive(args[i],value)) { std::cerr << "Expected a finite positive number.\n"; return 2; }
            (arg=="--meters-per-unit"?options.sourceMetersPerUnit:options.ticksPerSecond)=value;
        } else { std::cerr << "Unknown option: " << arg << '\n'; return 2; }
    }
    const auto input=fs::u8path(args[1]);
    if(command=="convert") return Convert(input,fs::u8path(args[2]),options,json);
    if(command=="batch") return Batch(input,fs::u8path(args[2]),options,json);
    Tool::Report report; Tool::Scene scene; scene.name=Tool::PathUTF8(input.filename());
    if(Lower(Tool::PathUTF8(input.extension()))==".glb") {
        std::string summary; const auto ok=Tool::ValidateGLB(input,report,&summary,json);
        if(ok) std::cout << summary << '\n'; else Print(scene,report,json);
        return ok?0:3;
    }
    options.optimize=false;
    const bool ok=Tool::Import(input,options,scene,report) && Tool::Process(scene,options,report);
    Print(scene,report,json); return ok?0:3;
}
int Entry(const std::vector<std::string>& args)
{
    try { std::locale::global(std::locale::classic()); return Run(args); }
    catch(const std::exception& error) {
        if(std::find(args.begin(),args.end(),"--json")!=args.end()) {
            Tool::Scene scene; Tool::Report report; report.Error("failure","tool",error.what()); Print(scene,report,true);
        } else std::cerr << "ERROR: " << error.what() << '\n';
        return 3;
    }
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv)
{
    std::vector<std::string> args; for(int i=1;i<argc;++i) args.push_back(Tool::PathUTF8(fs::path(argv[i]))); return Entry(args);
}
#else
int main(int argc,char** argv)
{
    std::vector<std::string> args(argv+1,argv+argc); return Entry(args);
}
#endif
