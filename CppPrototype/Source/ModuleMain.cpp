//
// iQs Wall Framer - selected wall probe milestone
//

#include "Prefix/StdAfx.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    constexpr const char* kPluginVWRIdentifier = "iQsWallFramer";
    constexpr const char* kMenuUniversalName   = "iQs Probe Selected Walls For Framing";
    constexpr const char* kHostRecordName      = "iQs_Host";
    constexpr const char* kFrameRecordName     = "iQs_StudWallFrame";
    constexpr const char* kExtruderRecordName  = "iQs Extruder V0.1";
    constexpr const char* kGeneratedClassName  = "iQs-Wall Framing";
    constexpr const char* kRequiredComponentName = "Timber Frame";
    constexpr const char* kSchema              = "iqs_wall_framing_probe_v0_1";
    constexpr const char* kGenerationSchema    = "iqs_stud_wall_frame_generation_v0_1";
    constexpr double kStudWidthMm              = 45.0;
    constexpr double kStudSpacingMm            = 450.0;
    constexpr double kPlateHeightMm            = 45.0;
    constexpr double kHeaderHeightMm           = 240.0;
    constexpr double kNoggingHeightMm          = 45.0;
    constexpr double kNoggingCentresMm         = 1350.0;
    constexpr size_t kBottomPlateCount         = 1;
    constexpr size_t kTopPlateCount            = 2;

    struct FramingSettings
    {
        TXString componentName = kRequiredComponentName;
        TXString generatedClassName = kGeneratedClassName;
        double studWidthMm = kStudWidthMm;
        double studDepthMm = 90.0;
        double studSpacingMm = kStudSpacingMm;
        double plateHeightMm = kPlateHeightMm;
        double plateWidthMm = 90.0;
        double headerHeightMm = kHeaderHeightMm;
        double headerWidthMm = 90.0;
        double noggingHeightMm = kNoggingHeightMm;
        double noggingWidthMm = 90.0;
        double sillHeightMm = kPlateHeightMm;
        double sillWidthMm = 90.0;
        double noggingCentresMm = kNoggingCentresMm;
        double noggingStaggerMm = kNoggingHeightMm;
        Sint32 bottomPlateCount = static_cast<Sint32>(kBottomPlateCount);
        Sint32 topPlateCount = static_cast<Sint32>(kTopPlateCount);
        bool detectDoors = true;
        bool detectWindows = true;
        Sint32 kingStudCount = 1;
        Sint32 trimmerStudCount = 1;
        bool generateNoggings = true;
        bool generateCornerStuds = true;
        bool resolveStudOverlaps = true;
        TXString bottomPlatePrefix = "BP";
        TXString topPlatePrefix = "TP";
        TXString studPrefix = "S";
        TXString noggingPrefix = "NOG";
        TXString windowSillPrefix = "WS";
        TXString lintelPrefix = "LIN";
        TXString doorHeadPrefix = "DH";
    };

    FramingSettings gSettings;

    std::string JsonEscape(const std::string& value)
    {
        std::ostringstream out;
        for (const unsigned char ch : value)
        {
            switch (ch)
            {
                case '"':  out << "\\\""; break;
                case '\\': out << "\\\\"; break;
                case '\b': out << "\\b";  break;
                case '\f': out << "\\f";  break;
                case '\n': out << "\\n";  break;
                case '\r': out << "\\r";  break;
                case '\t': out << "\\t";  break;
                default:
                    if (ch < 0x20)
                    {
                        out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                            << static_cast<int>(ch) << std::dec;
                    }
                    else
                    {
                        out << ch;
                    }
            }
        }
        return out.str();
    }

    std::string JsonString(const TXString& value)
    {
        return "\"" + JsonEscape(value.GetCharPtr()) + "\"";
    }

    std::string JsonString(const std::string& value)
    {
        return "\"" + JsonEscape(value) + "\"";
    }

    std::string Num(double value)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(6) << value;
        return out.str();
    }

    std::string Bool(bool value)
    {
        return value ? "true" : "false";
    }

    std::time_t Now()
    {
        return std::time(nullptr);
    }

    std::string Timestamp()
    {
        std::time_t now = Now();
        std::tm localTime{};
        localtime_r(&now, &localTime);
        char buffer[32] = {};
        std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &localTime);
        return buffer;
    }

    std::filesystem::path ProbeOutputDir()
    {
        const char* home = std::getenv("HOME");
        std::filesystem::path path = home ? std::filesystem::path(home) : std::filesystem::path("/tmp");
        path /= "Documents/Develop/iQs/20 VW Framing Tool/probe-output";
        std::filesystem::create_directories(path);
        return path;
    }

    std::string NewUUID()
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<unsigned int> byteDist(0, 255);
        unsigned char bytes[16] = {};
        for (auto& byte : bytes) { byte = static_cast<unsigned char>(byteDist(gen)); }
        bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
        bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);

        std::ostringstream out;
        out << std::hex << std::setfill('0');
        for (size_t i = 0; i < 16; ++i)
        {
            out << std::setw(2) << static_cast<unsigned int>(bytes[i]);
            if (i == 3 || i == 5 || i == 7 || i == 9) { out << "-"; }
        }
        return out.str();
    }

    void EnsureTextField(TFormatHandler& format, const TXString& fieldName,
                         const TXString& defaultValue = "")
    {
        if (format.GetFieldIndex(fieldName) != 0) { return; }
        TRecordItem item(kFieldText);
        item.SetFieldValueAsString(defaultValue);
        format.AddField(fieldName, item, true);
    }

    void EnsureHostRecordFormat()
    {
        TFormatHandler format(kHostRecordName);
        EnsureTextField(format, "iqs_uuid");
        EnsureTextField(format, "source_type", "VW_WALL_PIO");
        EnsureTextField(format, "last_framed_unix");
    }

    void EnsureFrameRecordFormat()
    {
        TFormatHandler format(kFrameRecordName);
        EnsureTextField(format, "iqs_uuid");
        EnsureTextField(format, "host_wall_uuid");
        EnsureTextField(format, "preset_code", "AU_TIMBER_90_MGP10_450");
        EnsureTextField(format, "source_component_mode");
        EnsureTextField(format, "source_component_ref");
        EnsureTextField(format, "wall_length_mm");
        EnsureTextField(format, "wall_height_mm");
        EnsureTextField(format, "stud_spacing_mm", "450");
        EnsureTextField(format, "member_count_total");
        EnsureTextField(format, "length_total_lm");
        EnsureTextField(format, "volume_total_m3");
        EnsureTextField(format, "last_generated_unix");
        EnsureTextField(format, "duplicate_status");
        EnsureTextField(format, "generated_class_name", kGeneratedClassName);
    }

    void EnsureExtruderRecordFormat()
    {
        TFormatHandler format(kExtruderRecordName);
        EnsureTextField(format, "script_version", "0.2-framer");
        EnsureTextField(format, "source_type");
        EnsureTextField(format, "object_name");
        EnsureTextField(format, "extrusion_mode");
        EnsureTextField(format, "created_unix");
        EnsureTextField(format, "extrusion_distance_doc");
        EnsureTextField(format, "extrusion_axis_semantic");
        EnsureTextField(format, "profile_dim_a_doc");
        EnsureTextField(format, "profile_dim_a_semantic");
        EnsureTextField(format, "profile_dim_b_doc");
        EnsureTextField(format, "profile_dim_b_semantic");
        EnsureTextField(format, "length_doc");
        EnsureTextField(format, "width_doc");
        EnsureTextField(format, "height_doc");
        EnsureTextField(format, "profile_area_doc2");
        EnsureTextField(format, "profile_perimeter_doc");
        EnsureTextField(format, "side_surface_area_doc2");
        EnsureTextField(format, "full_surface_area_doc2");
        EnsureTextField(format, "volume_doc3");
        EnsureTextField(format, "native_surface_area_doc2");
        EnsureTextField(format, "iqs_export_schema", "iqs_extruder_v0_1");
        EnsureTextField(format, "iqs_length_m");
        EnsureTextField(format, "iqs_width_m");
        EnsureTextField(format, "iqs_height_m");
        EnsureTextField(format, "iqs_profile_area_m2");
        EnsureTextField(format, "iqs_profile_perimeter_m");
        EnsureTextField(format, "iqs_side_surface_area_m2");
        EnsureTextField(format, "iqs_full_surface_area_m2");
        EnsureTextField(format, "iqs_volume_m3");
        EnsureTextField(format, "iqs_native_surface_area_m2");
        EnsureTextField(format, "iqs_native_volume_m3");
        EnsureTextField(format, "iqs_last_refresh_unix");
        EnsureTextField(format, "object_material");
        EnsureTextField(format, "volume_material");
        EnsureTextField(format, "geometry_source");
        EnsureTextField(format, "notes");
    }

    void SetRecordText(MCObjectHandle recordHandle, const TXString& fieldName,
                       const TXString& value)
    {
        if (!recordHandle) { return; }
        TRecordHandler record(recordHandle);
        const short fieldIndex = record.GetFieldIndex(fieldName);
        if (fieldIndex == 0) { return; }
        TRecordItem item(kFieldText);
        item.SetFieldValueAsString(value);
        record.SetFieldObject(fieldIndex, item);
    }

    TXString GetRecordText(MCObjectHandle recordHandle, const TXString& fieldName)
    {
        if (!recordHandle) { return ""; }
        TRecordHandler record(recordHandle);
        const short fieldIndex = record.GetFieldIndex(fieldName);
        if (fieldIndex == 0) { return ""; }
        TRecordItem item(record.GetFieldStyle(fieldIndex));
        if (!record.GetFieldObject(fieldIndex, item)) { return ""; }
        TXString value;
        item.GetFieldValueAsString(value);
        return value;
    }

    TXString EnsureHostUUID(MCObjectHandle wall)
    {
        EnsureHostRecordFormat();
        MCObjectHandle record =
            VWFC::VWObjects::VWRecordObj::GetRecordObject(wall, kHostRecordName);
        if (!record)
        {
            TFormatHandler format(kHostRecordName);
            record = format.AttachRecordToObject(wall);
        }

        TXString uuid = GetRecordText(record, "iqs_uuid");
        if (uuid.IsEmpty())
        {
            uuid = NewUUID().c_str();
            SetRecordText(record, "iqs_uuid", uuid);
        }
        SetRecordText(record, "source_type", "VW_WALL_PIO");
        SetRecordText(record, "last_framed_unix", TXString::ToStringInt(static_cast<Sint32>(Now())));
        return uuid;
    }

    void EnsureUniqueSelectedHostUUIDs(const std::vector<MCObjectHandle>& walls)
    {
        std::set<std::string> usedUUIDs;
        for (MCObjectHandle wall : walls)
        {
            TXString uuid = EnsureHostUUID(wall);
            if (usedUUIDs.insert(uuid.GetCharPtr()).second) { continue; }

            MCObjectHandle record =
                VWFC::VWObjects::VWRecordObj::GetRecordObject(wall, kHostRecordName);
            uuid = NewUUID().c_str();
            SetRecordText(record, "iqs_uuid", uuid);
            usedUUIDs.insert(uuid.GetCharPtr());
        }
    }

    MCObjectHandle AttachFrameRecord(MCObjectHandle frame)
    {
        EnsureFrameRecordFormat();
        MCObjectHandle record =
            VWFC::VWObjects::VWRecordObj::GetRecordObject(frame, kFrameRecordName);
        if (record) { return record; }
        TFormatHandler format(kFrameRecordName);
        return format.AttachRecordToObject(frame);
    }

    MCObjectHandle AttachExtruderRecord(MCObjectHandle member)
    {
        EnsureExtruderRecordFormat();
        MCObjectHandle record =
            VWFC::VWObjects::VWRecordObj::GetRecordObject(member, kExtruderRecordName);
        if (record) { return record; }
        TFormatHandler format(kExtruderRecordName);
        return format.AttachRecordToObject(member);
    }

    TXString ObjectName(MCObjectHandle object)
    {
        TXString name;
        if (object) { gSDK->GetObjectName(object, name); }
        return name;
    }

    TXString ClassName(MCObjectHandle object)
    {
        TXString name;
        if (object)
        {
            const InternalIndex classIndex = gSDK->GetObjectClass(object);
            gSDK->ClassIDToName(classIndex, name);
        }
        return name;
    }

    TXString LayerName(MCObjectHandle object)
    {
        MCObjectHandle parent = object;
        for (size_t depth = 0; parent && depth < 32; ++depth)
        {
            if (gSDK->GetObjectTypeN(parent) == kLayerNode) { return ObjectName(parent); }
            parent = ::GS_ParentObject(gCBP, parent);
        }
        return "";
    }

    InternalIndex GeneratedClassID()
    {
        const InternalIndex classID = gSDK->AddClass(gSettings.generatedClassName);
        gSDK->SetClassVisibility(classID, true);
        return classID;
    }

    std::string HandleText(MCObjectHandle object)
    {
        std::ostringstream out;
        out << "0x" << std::hex << reinterpret_cast<uintptr_t>(object);
        return out.str();
    }

    double Mm(double worldCoord)
    {
        return worldCoord;
    }

    double Distance(const VWFC::Math::VWPoint2D& a, const VWFC::Math::VWPoint2D& b)
    {
        const double dx = b.x - a.x;
        const double dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    struct FramingComponent
    {
        bool found = false;
        size_t index = 0;
        TXString name;
        TXString className;
        double depthMm = 0.0;
        double centerOffsetMm = 0.0;
        double topOffsetMm = 0.0;
        double bottomOffsetMm = 0.0;
    };

    struct FrameMember
    {
        std::string id;
        std::string type;
        double lengthMm = 0.0;
        double widthMm = 0.0;
        double depthMm = 0.0;
        double stationMm = 0.0;
        double zStartMm = 0.0;
        double zEndMm = 0.0;
        double extrusionDistanceMm = 0.0;
        double profileDimAMm = 0.0;
        double profileDimBMm = 0.0;
        TXString extrusionAxisSemantic;
        TXString profileDimASemantic;
        TXString profileDimBSemantic;
    };

    struct GeneratedFrame
    {
        MCObjectHandle group = nullptr;
        TXString frameUUID;
        TXString hostUUID;
        TXString sourceComponentMode;
        TXString sourceComponentRef;
        double wallLengthMm = 0.0;
        double wallHeightMm = 0.0;
        double frameDepthMm = 0.0;
        size_t studCount = 0;
        std::vector<FrameMember> members;
    };

    struct WallOpening
    {
        std::string type;
        double stationMm = 0.0;
        double widthMm = 0.0;
        double bottomMm = 0.0;
        double topMm = 0.0;
    };

    struct PlateExtents
    {
        double startStationMm = 0.0;
        double endStationMm = 0.0;
        std::vector<double> cornerStudStationsMm;
    };

    struct WallVerticalProfile
    {
        double bottomStartMm = 0.0;
        double bottomEndMm = 0.0;
        double topStartMm = 0.0;
        double topEndMm = 0.0;
    };

    double Interpolate(double start, double end, double stationMm, double wallLengthMm)
    {
        if (wallLengthMm <= 0.0) { return start; }
        return start + (end - start) * stationMm / wallLengthMm;
    }

    double Slope(double start, double end, double wallLengthMm)
    {
        return wallLengthMm <= 0.0 ? 0.0 : (end - start) / wallLengthMm;
    }

    double GrossVerticalLength(double centrelineLengthMm, double memberWidthMm,
                               double bottomSlope, double topSlope)
    {
        return centrelineLengthMm +
               memberWidthMm * (std::abs(bottomSlope) + std::abs(topSlope)) / 2.0;
    }

    double GrossVerticalBottom(double centrelineBottomMm, double memberWidthMm,
                               double bottomSlope)
    {
        return centrelineBottomMm - memberWidthMm * std::abs(bottomSlope) / 2.0;
    }

    double EntityOffsetZ(MCObjectHandle object)
    {
        if (!object) { return 0.0; }
        TransformMatrix matrix;
        ::GS_GetEntityMatrix(gCBP, object, matrix);
        return matrix.P().z;
    }

    bool GetLinearWallProfile(const VWFC::VWObjects::VWWallObj& wallObj,
                              WallVerticalProfile& profile)
    {
        VWFC::VWObjects::TWallPeakBreaksArray peaks;
        wallObj.EnumerateBreaks(peaks);
        if (!peaks.empty()) { return false; }

        wallObj.GetHeights(profile.topStartMm, profile.bottomStartMm,
                           profile.topEndMm, profile.bottomEndMm);
        return true;
    }

    bool IsSupportedWallForV1(const VWFC::VWObjects::VWWallObj& wallObj)
    {
        WallVerticalProfile profile;
        return !wallObj.IsRound() && GetLinearWallProfile(wallObj, profile);
    }

    void SetSemanticDimensionFields(MCObjectHandle recordHandle,
                                    const TXString& axisLabel,
                                    double distanceDoc,
                                    const TXString& dimALabel,
                                    double dimADoc,
                                    const TXString& dimBLabel,
                                    double dimBDoc)
    {
        auto SemanticVal = [&](const TXString& target) -> TXString
        {
            if (target == axisLabel) return Num(distanceDoc).c_str();
            if (target == dimALabel) return Num(dimADoc).c_str();
            if (target == dimBLabel) return Num(dimBDoc).c_str();
            return "";
        };

        SetRecordText(recordHandle, "extrusion_axis_semantic", axisLabel);
        SetRecordText(recordHandle, "profile_dim_a_semantic",  dimALabel);
        SetRecordText(recordHandle, "profile_dim_b_semantic",  dimBLabel);
        SetRecordText(recordHandle, "length_doc",              SemanticVal("Length"));
        SetRecordText(recordHandle, "width_doc",               SemanticVal("Width"));
        SetRecordText(recordHandle, "height_doc",              SemanticVal("Height"));
    }

    void SetMetricExportFields(MCObjectHandle recordHandle,
                               const TXString& axisLabel,
                               double distanceDoc,
                               const TXString& dimALabel,
                               double dimADoc,
                               const TXString& dimBLabel,
                               double dimBDoc,
                               double profileAreaDoc2,
                               double profilePerimeterDoc,
                               double sideSurfaceAreaDoc2,
                               double fullSurfaceAreaDoc2,
                               double volumeDoc3,
                               double nativeSurfaceM2,
                               double nativeVolumeM3)
    {
        auto SemanticMeters = [&](const TXString& target) -> TXString
        {
            if (target == axisLabel) return Num(distanceDoc / 1000.0).c_str();
            if (target == dimALabel) return Num(dimADoc / 1000.0).c_str();
            if (target == dimBLabel) return Num(dimBDoc / 1000.0).c_str();
            return "";
        };

        SetRecordText(recordHandle, "iqs_export_schema",          "iqs_extruder_v0_1");
        SetRecordText(recordHandle, "iqs_length_m",               SemanticMeters("Length"));
        SetRecordText(recordHandle, "iqs_width_m",                SemanticMeters("Width"));
        SetRecordText(recordHandle, "iqs_height_m",               SemanticMeters("Height"));
        SetRecordText(recordHandle, "iqs_profile_area_m2",        Num(profileAreaDoc2 / 1000000.0).c_str());
        SetRecordText(recordHandle, "iqs_profile_perimeter_m",    Num(profilePerimeterDoc / 1000.0).c_str());
        SetRecordText(recordHandle, "iqs_side_surface_area_m2",   Num(sideSurfaceAreaDoc2 / 1000000.0).c_str());
        SetRecordText(recordHandle, "iqs_full_surface_area_m2",   Num(fullSurfaceAreaDoc2 / 1000000.0).c_str());
        SetRecordText(recordHandle, "iqs_volume_m3",              Num(volumeDoc3 / 1000000000.0).c_str());
        SetRecordText(recordHandle, "iqs_native_surface_area_m2", Num(nativeSurfaceM2).c_str());
        SetRecordText(recordHandle, "iqs_native_volume_m3",       Num(nativeVolumeM3).c_str());
    }

    void TagExtruderSemanticMember(MCObjectHandle handle, const FrameMember& member)
    {
        if (!handle) { return; }

        const double profileArea = member.profileDimAMm * member.profileDimBMm;
        const double profilePerimeter = 2.0 * (member.profileDimAMm + member.profileDimBMm);
        const double sideArea = profilePerimeter * member.extrusionDistanceMm;
        const double fullArea = sideArea + 2.0 * profileArea;
        const double volume = profileArea * member.extrusionDistanceMm;

        MCObjectHandle record = AttachExtruderRecord(handle);
        SetRecordText(record, "script_version", "0.2-framer");
        SetRecordText(record, "source_type", "IQS_STUD_WALL_FRAME_MEMBER");
        SetRecordText(record, "object_name", member.id.c_str());
        SetRecordText(record, "extrusion_mode", "created_by_wall_framer");
        SetRecordText(record, "created_unix", TXString::ToStringInt(static_cast<Sint32>(Now())));
        SetRecordText(record, "extrusion_distance_doc", Num(member.extrusionDistanceMm).c_str());
        SetRecordText(record, "profile_dim_a_doc", Num(member.profileDimAMm).c_str());
        SetRecordText(record, "profile_dim_b_doc", Num(member.profileDimBMm).c_str());
        SetSemanticDimensionFields(record, member.extrusionAxisSemantic, member.extrusionDistanceMm,
                                   member.profileDimASemantic, member.profileDimAMm,
                                   member.profileDimBSemantic, member.profileDimBMm);
        SetRecordText(record, "profile_area_doc2", Num(profileArea).c_str());
        SetRecordText(record, "profile_perimeter_doc", Num(profilePerimeter).c_str());
        SetRecordText(record, "side_surface_area_doc2", Num(sideArea).c_str());
        SetRecordText(record, "full_surface_area_doc2", Num(fullArea).c_str());
        SetRecordText(record, "volume_doc3", Num(volume).c_str());
        SetRecordText(record, "native_surface_area_doc2", Num(gSDK->ObjSurfaceArea(handle)).c_str());
        SetMetricExportFields(record, member.extrusionAxisSemantic, member.extrusionDistanceMm,
                              member.profileDimASemantic, member.profileDimAMm,
                              member.profileDimBSemantic, member.profileDimBMm,
                              profileArea, profilePerimeter, sideArea, fullArea, volume,
                              static_cast<double>(gSDK->ObjSurfaceAreaInWorldCoords(handle)) / 1000000.0,
                              static_cast<double>(gSDK->ObjVolumeInWorldCoords(handle)) / 1000000000.0);
        SetRecordText(record, "iqs_last_refresh_unix", TXString::ToStringInt(static_cast<Sint32>(Now())));
        SetRecordText(record, "geometry_source", "iqs_wall_framer_rectangular_extrude");
        SetRecordText(record, "notes", "Semantic dimensions assigned automatically by iQs Wall Framer");
    }

    std::vector<MCObjectHandle> FindLinkedFrames(const TXString& hostUUID)
    {
        std::vector<MCObjectHandle> frames;
        gSDK->ForEachObjectN(allDrawing, [&](MCObjectHandle object) {
            MCObjectHandle record =
                VWFC::VWObjects::VWRecordObj::GetRecordObject(object, kFrameRecordName);
            if (record && GetRecordText(record, "host_wall_uuid") == hostUUID)
            {
                frames.push_back(object);
            }
        });
        return frames;
    }

    void ApplyDuplicateWarningColor(MCObjectHandle object)
    {
        if (!object) { return; }

        RGBColor rgb{ 65535, 0, 0 };
        ColorRef red = 0;
        gSDK->RGBToColorIndex(rgb, red);
        ObjectColorType colors{ red, red, red, red };
        gSDK->SetColor(object, colors);
        gSDK->SetFillPat(object, 1);

        for (MCObjectHandle child = gSDK->FirstMemberObj(object);
             child;
             child = gSDK->NextObject(child))
        {
            ApplyDuplicateWarningColor(child);
        }

        gSDK->ResetObject(object);
    }

    void FlagDuplicateFrame(MCObjectHandle frame)
    {
        ApplyDuplicateWarningColor(frame);
        MCObjectHandle record =
            VWFC::VWObjects::VWRecordObj::GetRecordObject(frame, kFrameRecordName);
        SetRecordText(record, "duplicate_status", "EXACT_HOST_DUPLICATE");
    }

    std::string Lower(const TXString& value)
    {
        std::string result = value.GetCharPtr();
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return result;
    }

    FramingComponent FindFramingComponent(MCObjectHandle wall,
                                          const VWFC::VWObjects::VWWallObj& wallObj)
    {
        FramingComponent result;
        double runningOffset = -wallObj.GetWidth() / 2.0;
        for (size_t i = 0; i < wallObj.GetComponentCount(); ++i)
        {
            const auto info = wallObj.GetComponentInfo(i);
            TXString className;
            gSDK->ClassIDToName(info.componentClass, className);
            const double centerOffset = -(runningOffset + info.width / 2.0);
            if (Lower(info.componentName) == Lower(gSettings.componentName))
            {
                result.found = true;
                result.index = i;
                result.name = info.componentName;
                result.className = className;
                result.depthMm = info.width;
                result.centerOffsetMm = centerOffset;
                WorldCoord offset = 0.0;
                if (gSDK->GetComponentOffsetFromWallTop(wall, static_cast<short>(i), offset))
                {
                    result.topOffsetMm = offset;
                }
                if (gSDK->GetComponentOffsetFromWallBottom(wall, static_cast<short>(i), offset))
                {
                    result.bottomOffsetMm = offset;
                }
                return result;
            }
            runningOffset += info.width;
        }

        return result;
    }

    VWFC::Math::VWPoint2D WallPoint(const VWFC::Math::VWPoint2D& start,
                                    const VWFC::Math::VWPoint2D& along,
                                    const VWFC::Math::VWPoint2D& normal,
                                    double stationMm, double offsetMm);

    bool SamePoint(const VWFC::Math::VWPoint2D& lhs, const VWFC::Math::VWPoint2D& rhs)
    {
        return Distance(lhs, rhs) <= 1.0;
    }

    bool LineIntersectionStation(const VWFC::Math::VWPoint2D& origin,
                                 const VWFC::Math::VWPoint2D& direction,
                                 const VWFC::Math::VWPoint2D& otherOrigin,
                                 const VWFC::Math::VWPoint2D& otherDirection,
                                 double& station)
    {
        const double cross =
            direction.x * otherDirection.y - direction.y * otherDirection.x;
        if (std::abs(cross) <= 0.000001) { return false; }
        const double dx = otherOrigin.x - origin.x;
        const double dy = otherOrigin.y - origin.y;
        station = (dx * otherDirection.y - dy * otherDirection.x) / cross;
        return true;
    }

    PlateExtents FindPlateExtents(MCObjectHandle wall,
                                  const std::vector<MCObjectHandle>& contextWalls)
    {
        VWFC::VWObjects::VWWallObj wallObj(wall);
        const auto start = wallObj.GetStartPoint();
        const auto end = wallObj.GetEndPoint();
        const double wallLength = Distance(start, end);
        PlateExtents result{ 0.0, wallLength };
        if (wallLength <= 0.0) { return result; }
        const auto wallIt = std::find(contextWalls.begin(), contextWalls.end(), wall);
        if (wallIt == contextWalls.end()) { return result; }
        const size_t wallIndex = static_cast<size_t>(std::distance(contextWalls.begin(), wallIt));

        const VWFC::Math::VWPoint2D along((end.x - start.x) / wallLength,
                                         (end.y - start.y) / wallLength);
        const VWFC::Math::VWPoint2D normal(-along.y, along.x);
        const FramingComponent component = FindFramingComponent(wall, wallObj);
        if (!component.found) { return result; }
        const auto componentStart = WallPoint(start, along, normal, 0.0, component.centerOffsetMm);

        for (size_t otherIndex = 0; otherIndex < contextWalls.size(); ++otherIndex)
        {
            if (otherIndex == wallIndex) { continue; }
            VWFC::VWObjects::VWWallObj otherObj(contextWalls[otherIndex]);
            if (otherObj.IsRound()) { continue; }

            const auto otherStart = otherObj.GetStartPoint();
            const auto otherEnd = otherObj.GetEndPoint();
            const double otherLength = Distance(otherStart, otherEnd);
            if (otherLength <= 0.0) { continue; }
            const VWFC::Math::VWPoint2D otherAlong(
                (otherEnd.x - otherStart.x) / otherLength,
                (otherEnd.y - otherStart.y) / otherLength);
            const VWFC::Math::VWPoint2D otherNormal(-otherAlong.y, otherAlong.x);
            const FramingComponent otherComponent =
                FindFramingComponent(contextWalls[otherIndex], otherObj);
            if (!otherComponent.found) { continue; }
            const auto otherComponentStart =
                WallPoint(otherStart, otherAlong, otherNormal, 0.0,
                          otherComponent.centerOffsetMm);

            bool atStart = false;
            if (SamePoint(start, otherStart) || SamePoint(start, otherEnd)) { atStart = true; }
            else if (!SamePoint(end, otherStart) && !SamePoint(end, otherEnd)) { continue; }

            double intersectionStation = 0.0;
            if (!LineIntersectionStation(componentStart, along, otherComponentStart,
                                         otherAlong, intersectionStation))
            {
                continue;
            }

            const bool through = wallIndex < otherIndex;
            const double outward = atStart ? -1.0 : 1.0;
            const double faceAdjustment =
                outward * otherComponent.depthMm / 2.0 * (through ? 1.0 : -1.0);
            if (atStart) { result.startStationMm = intersectionStation + faceAdjustment; }
            else { result.endStationMm = intersectionStation + faceAdjustment; }
            if (through)
            {
                const double cornerStudStation =
                    atStart
                        ? result.startStationMm + otherComponent.depthMm + gSettings.studWidthMm / 2.0
                        : result.endStationMm - otherComponent.depthMm - gSettings.studWidthMm / 2.0;
                if (gSettings.generateCornerStuds)
                {
                    result.cornerStudStationsMm.push_back(cornerStudStation);
                }
            }
        }
        return result;
    }

    VWFC::Math::VWPoint2D WallPoint(const VWFC::Math::VWPoint2D& start,
                                    const VWFC::Math::VWPoint2D& along,
                                    const VWFC::Math::VWPoint2D& normal,
                                    double stationMm, double offsetMm)
    {
        return VWFC::Math::VWPoint2D(start.x + along.x * stationMm + normal.x * offsetMm,
                                    start.y + along.y * stationMm + normal.y * offsetMm);
    }

    MCObjectHandle AddRectangularMember(VWFC::VWObjects::VWGroupObj& group,
                                        const VWFC::Math::VWPoint2D& wallStart,
                                        const VWFC::Math::VWPoint2D& along,
                                        const VWFC::Math::VWPoint2D& normal,
                                        double stationStartMm, double alongLengthMm,
                                        double centerOffsetMm, double depthMm,
                                        double zStartMm, double zHeightMm)
    {
        if (alongLengthMm <= 0.0 || depthMm <= 0.0 || zHeightMm <= 0.0) { return nullptr; }

        const double halfDepth = depthMm / 2.0;
        VWFC::Math::VWPolygon2D profile;
        profile.AddVertex(WallPoint(wallStart, along, normal, stationStartMm, centerOffsetMm - halfDepth));
        profile.AddVertex(WallPoint(wallStart, along, normal, stationStartMm + alongLengthMm, centerOffsetMm - halfDepth));
        profile.AddVertex(WallPoint(wallStart, along, normal, stationStartMm + alongLengthMm, centerOffsetMm + halfDepth));
        profile.AddVertex(WallPoint(wallStart, along, normal, stationStartMm, centerOffsetMm + halfDepth));
        profile.SetClosed(true);

        VWFC::VWObjects::VWExtrudeObj member(profile, zStartMm, zHeightMm);
        MCObjectHandle handle = member;
        group.AddObject(handle);
        gSDK->SetObjectClass(handle, GeneratedClassID());
        gSDK->ResetObject(handle);
        return handle;
    }

    MCObjectHandle AddSlopedPlateMember(VWFC::VWObjects::VWGroupObj& group,
                                        const VWFC::Math::VWPoint2D& wallStart,
                                        const VWFC::Math::VWPoint2D& along,
                                        const VWFC::Math::VWPoint2D& normal,
                                        double stationStartMm, double alongLengthMm,
                                        double centerOffsetMm, double depthMm,
                                        double axisStartZMm, double axisEndZMm,
                                        double plateHeightMm)
    {
        if (alongLengthMm <= 0.0 || depthMm <= 0.0 || plateHeightMm <= 0.0) { return nullptr; }

        const auto start2D = WallPoint(wallStart, along, normal, stationStartMm, centerOffsetMm);
        const auto end2D =
            WallPoint(wallStart, along, normal, stationStartMm + alongLengthMm, centerOffsetMm);
        const double halfDepth = depthMm / 2.0;
        const double halfHeight = plateHeightMm / 2.0;

        VWFC::Math::VWPolygon2D profile;
        profile.AddVertex(VWFC::Math::VWPoint2D(-halfDepth, -halfHeight));
        profile.AddVertex(VWFC::Math::VWPoint2D(halfDepth, -halfHeight));
        profile.AddVertex(VWFC::Math::VWPoint2D(halfDepth, halfHeight));
        profile.AddVertex(VWFC::Math::VWPoint2D(-halfDepth, halfHeight));
        profile.SetClosed(true);

        VWFC::VWObjects::VWPolygon2DObj profileObj(profile);
        VWFC::VWObjects::VWExtrudeObj member(
            profileObj,
            VWFC::Math::VWPoint3D(start2D.x, start2D.y, axisStartZMm),
            VWFC::Math::VWPoint3D(end2D.x, end2D.y, axisEndZMm),
            VWFC::Math::VWPoint3D(0.0, 0.0, 1.0));
        MCObjectHandle handle = member;
        group.AddObject(handle);
        gSDK->SetObjectClass(handle, GeneratedClassID());
        gSDK->ResetObject(handle);
        return handle;
    }

    std::string MemberJson(const FrameMember& member)
    {
        return "{\"member_id\":" + JsonString(member.id) +
               ",\"member_type\":" + JsonString(member.type) +
               ",\"size_code\":\"" + Num(member.depthMm) + "x" + Num(member.widthMm) + "\"" +
               ",\"width_mm\":" + Num(member.widthMm) +
               ",\"depth_mm\":" + Num(member.depthMm) +
               ",\"length_mm\":" + Num(member.lengthMm) +
               ",\"station_mm\":" + Num(member.stationMm) +
               ",\"z_start_mm\":" + Num(member.zStartMm) +
               ",\"z_end_mm\":" + Num(member.zEndMm) +
               ",\"extrusion_distance_mm\":" + Num(member.extrusionDistanceMm) +
               ",\"extrusion_axis_semantic\":" + JsonString(member.extrusionAxisSemantic) +
               ",\"profile_dim_a_mm\":" + Num(member.profileDimAMm) +
               ",\"profile_dim_a_semantic\":" + JsonString(member.profileDimASemantic) +
               ",\"profile_dim_b_mm\":" + Num(member.profileDimBMm) +
               ",\"profile_dim_b_semantic\":" + JsonString(member.profileDimBSemantic) + "}";
    }

    std::string MembersJson(const std::vector<FrameMember>& members)
    {
        std::ostringstream out;
        out << "[";
        for (size_t i = 0; i < members.size(); ++i)
        {
            if (i > 0) { out << ","; }
            out << MemberJson(members[i]);
        }
        out << "]";
        return out.str();
    }

    std::vector<WallOpening> FindWallOpenings(const VWFC::VWObjects::VWWallObj& wallObj,
                                              double frameBottomStart,
                                              double frameBottomEnd,
                                              double wallLength)
    {
        std::vector<WallOpening> openings;
        VWFC::VWObjects::TWallSymbolBreaksArray breaks;
        wallObj.EnumerateBreaks(breaks);
        for (const auto& brk : breaks)
        {
            MCObjectHandle insert = brk.symBreak.theSymbol;
            if (!insert || !VWFC::VWObjects::VWParametricObj::IsParametricObject(insert)) { continue; }

            VWFC::VWObjects::VWParametricObj parametric(insert);
            const TXString parametricName = parametric.GetParametricName();
            WallOpening opening;
            opening.stationMm = brk.offset;
            const double frameBottom =
                Interpolate(frameBottomStart, frameBottomEnd, opening.stationMm, wallLength);
            if (parametricName.EqualNoCase("Door"))
            {
                if (!gSettings.detectDoors) { continue; }
                opening.type = "DOOR";
                opening.widthMm = parametric.GetParamReal("ROWidth");
                opening.bottomMm = frameBottom + EntityOffsetZ(insert);
                opening.topMm = opening.bottomMm + parametric.GetParamReal("ROHeight");
            }
            else if (parametricName.EqualNoCase("Window"))
            {
                if (!gSettings.detectWindows) { continue; }
                opening.type = "WINDOW";
                opening.widthMm = parametric.GetParamReal("RoughOpenWIdth");
                const double heightMm = parametric.GetParamReal("RoughOpenHeight");
                const double elevationMm = parametric.GetParamReal("Elevation");
                const TXString elevationSetAt = parametric.GetParamString("ElevationSetAt");
                if (elevationSetAt.EqualNoCase("Head of Window"))
                {
                    opening.topMm = frameBottom + elevationMm + EntityOffsetZ(insert);
                    opening.bottomMm = opening.topMm - heightMm;
                }
                else
                {
                    opening.bottomMm = frameBottom + elevationMm + EntityOffsetZ(insert);
                    opening.topMm = opening.bottomMm + heightMm;
                }
            }
            else
            {
                continue;
            }

            if (opening.widthMm <= 0.0 || opening.topMm <= opening.bottomMm) { continue; }
            const bool duplicate = std::any_of(openings.begin(), openings.end(),
                                               [&](const WallOpening& existing) {
                return existing.type == opening.type &&
                       std::abs(existing.stationMm - opening.stationMm) < 0.001 &&
                       std::abs(existing.widthMm - opening.widthMm) < 0.001 &&
                       std::abs(existing.bottomMm - opening.bottomMm) < 0.001 &&
                       std::abs(existing.topMm - opening.topMm) < 0.001;
            });
            if (!duplicate) { openings.push_back(opening); }
        }
        return openings;
    }

    GeneratedFrame GenerateSimpleFrame(MCObjectHandle wall, const PlateExtents& plateExtents)
    {
        const double kStudWidthMm = gSettings.studWidthMm;
        const double kStudSpacingMm = gSettings.studSpacingMm;
        const double kPlateHeightMm = gSettings.plateHeightMm;
        const double kHeaderHeightMm = gSettings.headerHeightMm;
        const double kNoggingHeightMm = gSettings.noggingHeightMm;
        const double kNoggingCentresMm = gSettings.noggingCentresMm;
        const size_t kBottomPlateCount = static_cast<size_t>(gSettings.bottomPlateCount);
        const size_t kTopPlateCount = static_cast<size_t>(gSettings.topPlateCount);
        GeneratedFrame result;
        VWFC::VWObjects::VWWallObj wallObj(wall);
        if (wallObj.IsRound()) { return result; }

        const auto start = wallObj.GetStartPoint();
        const auto end = wallObj.GetEndPoint();
        const double wallLength = Distance(start, end);
        if (wallLength <= 0.0) { return result; }

        WallVerticalProfile wallProfile;
        if (!GetLinearWallProfile(wallObj, wallProfile)) { return result; }

        const VWFC::Math::VWPoint2D along((end.x - start.x) / wallLength,
                                         (end.y - start.y) / wallLength);
        const VWFC::Math::VWPoint2D normal(-along.y, along.x);

        const FramingComponent component = FindFramingComponent(wall, wallObj);
        if (!component.found) { return result; }
        wallProfile.bottomStartMm += component.bottomOffsetMm;
        wallProfile.bottomEndMm += component.bottomOffsetMm;
        wallProfile.topStartMm += component.topOffsetMm;
        wallProfile.topEndMm += component.topOffsetMm;
        const double bottomSlope =
            Slope(wallProfile.bottomStartMm, wallProfile.bottomEndMm, wallLength);
        const double topSlope =
            Slope(wallProfile.topStartMm, wallProfile.topEndMm, wallLength);
        const double frameHeight =
            std::max(wallProfile.topStartMm, wallProfile.topEndMm) -
            std::min(wallProfile.bottomStartMm, wallProfile.bottomEndMm);
        if (component.depthMm <= 0.0 || frameHeight <= 0.0) { return result; }
        const std::vector<WallOpening> openings =
            FindWallOpenings(wallObj, wallProfile.bottomStartMm,
                             wallProfile.bottomEndMm, wallLength);

        VWFC::VWObjects::VWGroupObj group;
        result.group = group;
        result.frameUUID = NewUUID().c_str();
        result.hostUUID = EnsureHostUUID(wall);
        result.sourceComponentMode = "COMPONENT_NAME";
        result.sourceComponentRef = component.name;
        result.wallLengthMm = wallLength;
        result.wallHeightMm = frameHeight;
        result.frameDepthMm = component.depthMm;

        const std::string frameName = "iQs_StudWallFrame_" + std::string(result.frameUUID.GetCharPtr());
        gSDK->SetObjectName(result.group, frameName.c_str());
        gSDK->SetObjectClass(result.group, GeneratedClassID());

        std::map<std::string, size_t> memberNameCounters;
        auto nextMemberName = [&](const TXString& prefix) {
            const std::string prefixText = prefix.GetCharPtr();
            std::ostringstream name;
            name << prefixText << "-" << std::setw(3) << std::setfill('0')
                 << ++memberNameCounters[prefixText];
            return name.str();
        };

        auto addMember = [&](const std::string& id, const std::string& type,
                             double stationStart, double alongLength,
                             double zStart, double zHeight, double geometryDepth,
                             double scheduleStation,
                             double memberLength, double memberWidth, double memberHeight,
                             const TXString& axisSemantic,
                             const TXString& profileDimASemantic,
                             const TXString& profileDimBSemantic) {
            MCObjectHandle memberHandle =
                AddRectangularMember(group, start, along, normal, stationStart, alongLength,
                                     component.centerOffsetMm, geometryDepth, zStart, zHeight);
            if (!memberHandle)
            {
                return;
            }
            const double exportExtrusionDistance =
                axisSemantic == "Length" ? memberLength : zHeight;
            FrameMember member{ id, type, memberLength, memberWidth, memberHeight, scheduleStation,
                                zStart, zStart + zHeight,
                                exportExtrusionDistance, alongLength, geometryDepth,
                                axisSemantic, profileDimASemantic, profileDimBSemantic };
            result.members.push_back(member);
            TagExtruderSemanticMember(memberHandle, member);
        };

        auto addSlopedPlate = [&](const std::string& id, const std::string& type,
                                  double axisStartZ, double axisEndZ) {
            const double stationStart = plateExtents.startStationMm;
            const double stationEnd = plateExtents.endStationMm;
            const double alongLength = stationEnd - stationStart;
            if (alongLength <= 0.0) { return; }
            const double trimmedAxisStartZ =
                Interpolate(axisStartZ, axisEndZ, stationStart, wallLength);
            const double trimmedAxisEndZ =
                Interpolate(axisStartZ, axisEndZ, stationEnd, wallLength);
            const double axisLength =
                std::sqrt(alongLength * alongLength +
                          (trimmedAxisEndZ - trimmedAxisStartZ) *
                              (trimmedAxisEndZ - trimmedAxisStartZ));
            if (axisLength <= 0.0) { return; }
            const double grossLength =
                axisLength + kPlateHeightMm *
                                 std::abs(trimmedAxisEndZ - trimmedAxisStartZ) / axisLength;
            const double slope =
                (trimmedAxisEndZ - trimmedAxisStartZ) / alongLength;
            const double stationExtension =
                kPlateHeightMm * std::abs(slope) / (2.0 * (1.0 + slope * slope));
            const double grossStationStart = stationStart - stationExtension;
            const double grossStationEnd = stationEnd + stationExtension;
            MCObjectHandle memberHandle =
                AddSlopedPlateMember(group, start, along, normal,
                                     grossStationStart, grossStationEnd - grossStationStart,
                                     component.centerOffsetMm, gSettings.plateWidthMm,
                                     Interpolate(axisStartZ, axisEndZ, grossStationStart, wallLength),
                                     Interpolate(axisStartZ, axisEndZ, grossStationEnd, wallLength),
                                     kPlateHeightMm);
            if (!memberHandle) { return; }

            FrameMember member{ id, type, grossLength, kPlateHeightMm, gSettings.plateWidthMm,
                                stationStart, trimmedAxisStartZ - kPlateHeightMm / 2.0,
                                trimmedAxisEndZ + kPlateHeightMm / 2.0,
                                grossLength, gSettings.plateWidthMm, kPlateHeightMm,
                                "Length", "Height", "Width" };
            result.members.push_back(member);
            TagExtruderSemanticMember(memberHandle, member);
        };

        for (size_t i = 0; i < kBottomPlateCount; ++i)
        {
            const double offset = (static_cast<double>(i) + 0.5) * kPlateHeightMm;
            addSlopedPlate(nextMemberName(gSettings.bottomPlatePrefix), "BOTTOM_PLATE",
                           wallProfile.bottomStartMm + offset,
                           wallProfile.bottomEndMm + offset);
        }
        for (size_t i = 0; i < kTopPlateCount; ++i)
        {
            const double offset = (static_cast<double>(i) + 0.5) * kPlateHeightMm;
            addSlopedPlate(nextMemberName(gSettings.topPlatePrefix), "TOP_PLATE",
                           wallProfile.topStartMm - offset, wallProfile.topEndMm - offset);
        }

        struct PendingVerticalMember
        {
            std::string id;
            std::string type;
            double stationMm = 0.0;
            double zStartMm = 0.0;
            double zHeightMm = 0.0;
            double memberLengthMm = 0.0;
            int priority = 0;
        };

        std::vector<PendingVerticalMember> pendingVerticalMembers;
        auto queueVerticalMember = [&](const std::string& id, const std::string& type,
                                       double station, double zStart, double zHeight,
                                       double memberLength, int priority) {
            if (zHeight <= 0.0) { return; }
            pendingVerticalMembers.push_back(
                { id, type, station, zStart, zHeight, memberLength, priority });
        };

        std::vector<double> stations;
        const double firstStudCenter = plateExtents.startStationMm + kStudWidthMm / 2.0;
        const double finalStudCenter = plateExtents.endStationMm - kStudWidthMm / 2.0;
        if (finalStudCenter < firstStudCenter) { return result; }
        stations.push_back(firstStudCenter);
        for (double station = firstStudCenter + kStudSpacingMm;
             station < finalStudCenter; station += kStudSpacingMm)
        {
            stations.push_back(station);
        }
        if (std::abs(stations.back() - finalStudCenter) > 0.001)
        {
            stations.push_back(finalStudCenter);
        }

        for (size_t i = 0; i < stations.size(); ++i)
        {
            const bool endStud = i == 0 || i + 1 == stations.size();
            const double studBottom =
                Interpolate(wallProfile.bottomStartMm, wallProfile.bottomEndMm,
                            stations[i], wallLength) +
                static_cast<double>(kBottomPlateCount) * kPlateHeightMm;
            const double studTop =
                Interpolate(wallProfile.topStartMm, wallProfile.topEndMm,
                            stations[i], wallLength) -
                static_cast<double>(kTopPlateCount) * kPlateHeightMm;
            const double clearStudHeight = studTop - studBottom;
            if (clearStudHeight <= 0.0) { continue; }
            const bool overlapsOpening =
                std::any_of(openings.begin(), openings.end(), [&](const WallOpening& opening) {
                    const double studLeft = stations[i] - kStudWidthMm / 2.0;
                    const double studRight = stations[i] + kStudWidthMm / 2.0;
                    const double openingLeft = opening.stationMm - opening.widthMm / 2.0;
                    const double openingRight = opening.stationMm + opening.widthMm / 2.0;
                    return studLeft < openingRight && studRight > openingLeft;
                });
            if (overlapsOpening && !endStud) { continue; }

            queueVerticalMember(
                "", endStud ? "END_STUD" : "STUD", stations[i],
                GrossVerticalBottom(studBottom, kStudWidthMm, bottomSlope),
                GrossVerticalLength(clearStudHeight, kStudWidthMm, bottomSlope, topSlope),
                GrossVerticalLength(clearStudHeight, kStudWidthMm, bottomSlope, topSlope),
                endStud ? 100 : 50);
        }

        for (double station : plateExtents.cornerStudStationsMm)
        {
            const double studBottom =
                Interpolate(wallProfile.bottomStartMm, wallProfile.bottomEndMm,
                            station, wallLength) +
                static_cast<double>(kBottomPlateCount) * kPlateHeightMm;
            const double studTop =
                Interpolate(wallProfile.topStartMm, wallProfile.topEndMm,
                            station, wallLength) -
                static_cast<double>(kTopPlateCount) * kPlateHeightMm;
            const double clearStudHeight = studTop - studBottom;
            if (clearStudHeight <= 0.0) { continue; }

            queueVerticalMember(
                "", "CORNER_STUD", station,
                GrossVerticalBottom(studBottom, kStudWidthMm, bottomSlope),
                GrossVerticalLength(clearStudHeight, kStudWidthMm, bottomSlope, topSlope),
                GrossVerticalLength(clearStudHeight, kStudWidthMm, bottomSlope, topSlope), 60);
        }

        for (size_t i = 0; i < openings.size(); ++i)
        {
            const WallOpening& opening = openings[i];
            const double openingLeft = opening.stationMm - opening.widthMm / 2.0;
            const double openingRight = opening.stationMm + opening.widthMm / 2.0;
            auto addOpeningStud = [&](const std::string& type,
                                      double stationStart, double scheduleStation,
                                      double requestedTop, double studTopSlope) {
                const double studBottom =
                    Interpolate(wallProfile.bottomStartMm, wallProfile.bottomEndMm,
                                scheduleStation, wallLength) +
                    static_cast<double>(kBottomPlateCount) * kPlateHeightMm;
                const double availableTop =
                    Interpolate(wallProfile.topStartMm, wallProfile.topEndMm,
                                scheduleStation, wallLength) -
                    static_cast<double>(kTopPlateCount) * kPlateHeightMm;
                const double studTop = std::min(requestedTop, availableTop);
                const double height = studTop - studBottom;
                if (height <= 0.0) { return; }
                queueVerticalMember(
                    "", type, scheduleStation,
                    GrossVerticalBottom(studBottom, kStudWidthMm, bottomSlope),
                    GrossVerticalLength(height, kStudWidthMm, bottomSlope, studTopSlope),
                    GrossVerticalLength(height, kStudWidthMm, bottomSlope, studTopSlope),
                    type == "TRIMMER_STUD" ? 80 : 70);
            };

            for (Sint32 i = 0; i < gSettings.trimmerStudCount; ++i)
            {
                addOpeningStud("TRIMMER_STUD",
                               openingLeft - (static_cast<double>(i) + 1.0) * kStudWidthMm,
                               openingLeft - (static_cast<double>(i) + 0.5) * kStudWidthMm,
                               opening.topMm, 0.0);
                addOpeningStud("TRIMMER_STUD",
                               openingRight + static_cast<double>(i) * kStudWidthMm,
                               openingRight + (static_cast<double>(i) + 0.5) * kStudWidthMm,
                               opening.topMm, 0.0);
            }
            for (Sint32 i = 0; i < gSettings.kingStudCount; ++i)
            {
                const double jambOffset =
                    (static_cast<double>(gSettings.trimmerStudCount + i) + 0.5) * kStudWidthMm;
                addOpeningStud("KING_STUD",
                               openingLeft - jambOffset - kStudWidthMm / 2.0,
                               openingLeft - jambOffset,
                               std::numeric_limits<double>::max(), topSlope);
                addOpeningStud("KING_STUD",
                               openingRight + jambOffset - kStudWidthMm / 2.0,
                               openingRight + jambOffset,
                               std::numeric_limits<double>::max(), topSlope);
            }

            const double trimmerPackWidth =
                static_cast<double>(gSettings.trimmerStudCount) * kStudWidthMm;
            const double headerStart = openingLeft - trimmerPackWidth;
            const double headerLength = opening.widthMm + 2.0 * trimmerPackWidth;
            addMember(nextMemberName(opening.type == "WINDOW"
                                         ? gSettings.lintelPrefix
                                         : gSettings.doorHeadPrefix),
                      "HEADER", headerStart, headerLength,
                      opening.topMm, kHeaderHeightMm, gSettings.headerWidthMm, opening.stationMm,
                      headerLength, gSettings.headerWidthMm, kHeaderHeightMm,
                      "Height", "Length", "Width");

            if (opening.type == "WINDOW")
            {
                addMember(nextMemberName(gSettings.windowSillPrefix), "SILL", openingLeft, opening.widthMm,
                          opening.bottomMm - gSettings.sillHeightMm, gSettings.sillHeightMm,
                          gSettings.sillWidthMm,
                          opening.stationMm,
                          opening.widthMm, gSettings.sillWidthMm, gSettings.sillHeightMm,
                          "Width", "Length", "Height");
            }

            const double upperCrippleBottom = opening.topMm + kHeaderHeightMm;
            for (double station : stations)
            {
                if (station <= openingLeft || station >= openingRight) { continue; }
                const double upperCrippleTop =
                    Interpolate(wallProfile.topStartMm, wallProfile.topEndMm,
                                station, wallLength) -
                    static_cast<double>(kTopPlateCount) * kPlateHeightMm;
                if (upperCrippleTop > upperCrippleBottom)
                {
                    const double height = upperCrippleTop - upperCrippleBottom;
                    addMember(nextMemberName(gSettings.studPrefix), "CRIPPLE_STUD_ABOVE",
                              station - kStudWidthMm / 2.0, kStudWidthMm,
                              upperCrippleBottom,
                              GrossVerticalLength(height, kStudWidthMm, 0.0, topSlope),
                              gSettings.studDepthMm,
                              station,
                              GrossVerticalLength(height, kStudWidthMm, 0.0, topSlope),
                              kStudWidthMm, gSettings.studDepthMm,
                              "Length", "Width", "Height");
                }
                const double lowerCrippleBottom =
                    Interpolate(wallProfile.bottomStartMm, wallProfile.bottomEndMm,
                                station, wallLength) +
                    static_cast<double>(kBottomPlateCount) * kPlateHeightMm;
                const double lowerCrippleTop = opening.bottomMm - gSettings.sillHeightMm;
                if (lowerCrippleTop > lowerCrippleBottom)
                {
                    const double height = lowerCrippleTop - lowerCrippleBottom;
                    addMember(nextMemberName(gSettings.studPrefix), "CRIPPLE_STUD_BELOW",
                              station - kStudWidthMm / 2.0, kStudWidthMm,
                              GrossVerticalBottom(lowerCrippleBottom, kStudWidthMm, bottomSlope),
                              GrossVerticalLength(height, kStudWidthMm, bottomSlope, 0.0),
                              gSettings.studDepthMm,
                              station,
                              GrossVerticalLength(height, kStudWidthMm, bottomSlope, 0.0),
                              kStudWidthMm, gSettings.studDepthMm,
                              "Length", "Width", "Height");
                }
            }
        }

        std::stable_sort(pendingVerticalMembers.begin(), pendingVerticalMembers.end(),
                         [](const PendingVerticalMember& lhs,
                            const PendingVerticalMember& rhs) {
                             return lhs.priority > rhs.priority;
                         });
        std::vector<PendingVerticalMember> acceptedVerticalMembers;
        for (const PendingVerticalMember& candidate : pendingVerticalMembers)
        {
            const double candidateLeft = candidate.stationMm - kStudWidthMm / 2.0;
            const double candidateRight = candidate.stationMm + kStudWidthMm / 2.0;
            const bool overlapsAccepted =
                std::any_of(acceptedVerticalMembers.begin(), acceptedVerticalMembers.end(),
                            [&](const PendingVerticalMember& accepted) {
                                const double acceptedLeft =
                                    accepted.stationMm - kStudWidthMm / 2.0;
                                const double acceptedRight =
                                    accepted.stationMm + kStudWidthMm / 2.0;
                                return candidateLeft < acceptedRight - 0.001 &&
                                       candidateRight > acceptedLeft + 0.001;
                            });
            if (gSettings.resolveStudOverlaps && overlapsAccepted) { continue; }

            acceptedVerticalMembers.push_back(candidate);
            addMember(nextMemberName(gSettings.studPrefix), candidate.type,
                      candidate.stationMm - kStudWidthMm / 2.0, kStudWidthMm,
                      candidate.zStartMm, candidate.zHeightMm, gSettings.studDepthMm,
                      candidate.stationMm,
                      candidate.memberLengthMm, kStudWidthMm, gSettings.studDepthMm,
                      "Length", "Width", "Height");
            ++result.studCount;
        }

        struct VerticalEdge
        {
            double left = 0.0;
            double right = 0.0;
            double bottom = 0.0;
            double top = 0.0;
        };

        std::vector<VerticalEdge> verticalEdges;
        for (const FrameMember& member : result.members)
        {
            if (member.type.find("STUD") == std::string::npos) { continue; }
            verticalEdges.push_back({ member.stationMm - member.widthMm / 2.0,
                                      member.stationMm + member.widthMm / 2.0,
                                      member.zStartMm, member.zEndMm });
        }
        std::sort(verticalEdges.begin(), verticalEdges.end(),
                  [](const VerticalEdge& lhs, const VerticalEdge& rhs) {
                      return lhs.left < rhs.left;
                  });

        std::vector<VerticalEdge> mergedEdges;
        for (const auto& edge : verticalEdges)
        {
            if (mergedEdges.empty() || edge.left > mergedEdges.back().right + 0.001)
            {
                mergedEdges.push_back(edge);
            }
            else
            {
                mergedEdges.back().right = std::max(mergedEdges.back().right, edge.right);
                mergedEdges.back().bottom = std::max(mergedEdges.back().bottom, edge.bottom);
                mergedEdges.back().top = std::min(mergedEdges.back().top, edge.top);
            }
        }

        const double highestFrameHeight =
            std::max(wallProfile.topStartMm - wallProfile.bottomStartMm,
                     wallProfile.topEndMm - wallProfile.bottomEndMm);
        const size_t noggingRowCount =
            gSettings.generateNoggings
                ? static_cast<size_t>(std::floor(highestFrameHeight / kNoggingCentresMm))
                : 0;
        for (size_t row = 1; row <= noggingRowCount; ++row)
        {
            size_t noggingIndex = 0;
            for (size_t i = 1; i < mergedEdges.size(); ++i)
            {
                const double gapStart = mergedEdges[i - 1].right;
                const double gapEnd = mergedEdges[i].left;
                const double gapLength = gapEnd - gapStart;
                if (gapLength <= 0.001) { continue; }

                const double noggingCentreZ =
                    Interpolate(wallProfile.bottomStartMm, wallProfile.bottomEndMm,
                                (gapStart + gapEnd) / 2.0, wallLength) +
                    static_cast<double>(row) * kNoggingCentresMm;
                const double noggingZ =
                    noggingIndex % 2 == 0 ? noggingCentreZ - gSettings.noggingStaggerMm
                                         : noggingCentreZ;
                const double noggingTop = noggingZ + kNoggingHeightMm;
                if (noggingZ < std::max(mergedEdges[i - 1].bottom, mergedEdges[i].bottom) ||
                    noggingTop > std::min(mergedEdges[i - 1].top, mergedEdges[i].top))
                {
                    continue;
                }

                const bool intersectsOpening =
                    std::any_of(openings.begin(), openings.end(), [&](const WallOpening& opening) {
                        const double openingLeft = opening.stationMm - opening.widthMm / 2.0;
                        const double openingRight = opening.stationMm + opening.widthMm / 2.0;
                        const bool horizontalOverlap = gapStart < openingRight && gapEnd > openingLeft;
                        const bool verticalOverlap = noggingZ < opening.topMm && noggingTop > opening.bottomMm;
                        return horizontalOverlap && verticalOverlap;
                    });
                if (intersectsOpening) { continue; }

                ++noggingIndex;
                addMember(nextMemberName(gSettings.noggingPrefix), "NOGGING", gapStart, gapLength,
                          noggingZ, kNoggingHeightMm, gSettings.noggingWidthMm,
                          (gapStart + gapEnd) / 2.0,
                          gapLength, gSettings.noggingWidthMm, kNoggingHeightMm,
                          "Width", "Length", "Height");
            }
        }

        MCObjectHandle record = AttachFrameRecord(result.group);
        double lengthTotalMm = 0.0;
        double volumeTotalMm3 = 0.0;
        for (const auto& member : result.members)
        {
            lengthTotalMm += member.lengthMm;
            volumeTotalMm3 += member.lengthMm * member.widthMm * member.depthMm;
        }
        SetRecordText(record, "iqs_uuid", result.frameUUID);
        SetRecordText(record, "host_wall_uuid", result.hostUUID);
        SetRecordText(record, "source_component_mode", result.sourceComponentMode);
        SetRecordText(record, "source_component_ref", result.sourceComponentRef);
        SetRecordText(record, "wall_length_mm", Num(result.wallLengthMm).c_str());
        SetRecordText(record, "wall_height_mm", Num(result.wallHeightMm).c_str());
        SetRecordText(record, "stud_spacing_mm", Num(kStudSpacingMm).c_str());
        SetRecordText(record, "member_count_total", TXString::ToStringInt(static_cast<Sint32>(result.members.size())));
        SetRecordText(record, "length_total_lm", Num(lengthTotalMm / 1000.0).c_str());
        SetRecordText(record, "volume_total_m3", Num(volumeTotalMm3 / 1000000000.0).c_str());
        SetRecordText(record, "last_generated_unix", TXString::ToStringInt(static_cast<Sint32>(Now())));
        SetRecordText(record, "duplicate_status", "OK");
        SetRecordText(record, "generated_class_name", gSettings.generatedClassName);
        gSDK->ResetObject(result.group);
        gSDK->SelectObject(result.group, true);

        WorldRect redrawBounds;
        if (gSDK->GetObjectBounds(result.group, redrawBounds))
        {
            ::GS_RedrawRect(gCBP, redrawBounds);
        }

        return result;
    }

    std::string GeneratedFrameJson(const GeneratedFrame& frame)
    {
        double lengthTotalMm = 0.0;
        double volumeTotalMm3 = 0.0;
        for (const auto& member : frame.members)
        {
            lengthTotalMm += member.lengthMm;
            volumeTotalMm3 += member.lengthMm * member.widthMm * member.depthMm;
        }
        return "{\"type\":\"iQs_StudWallFrame\""
               ",\"iqs_uuid\":" + JsonString(frame.frameUUID) +
               ",\"host_wall_uuid\":" + JsonString(frame.hostUUID) +
               ",\"preset_code\":\"AU_TIMBER_90_MGP10_450\"" +
               ",\"source_component_mode\":" + JsonString(frame.sourceComponentMode) +
               ",\"source_component_ref\":" + JsonString(frame.sourceComponentRef) +
               ",\"wall_length_mm\":" + Num(frame.wallLengthMm) +
               ",\"wall_height_mm\":" + Num(frame.wallHeightMm) +
               ",\"frame_depth_mm\":" + Num(frame.frameDepthMm) +
               ",\"stud_spacing_mm\":" + Num(gSettings.studSpacingMm) +
               ",\"top_plate_count\":" + std::to_string(gSettings.topPlateCount) +
               ",\"bottom_plate_count\":" + std::to_string(gSettings.bottomPlateCount) +
               ",\"stud_count\":" + std::to_string(frame.studCount) +
               ",\"member_count_total\":" + std::to_string(frame.members.size()) +
               ",\"length_total_lm\":" + Num(lengthTotalMm / 1000.0) +
               ",\"volume_total_m3\":" + Num(volumeTotalMm3 / 1000000000.0) +
               ",\"members\":" + MembersJson(frame.members) + "}";
    }

    std::string PointJson(const VWFC::Math::VWPoint2D& point)
    {
        return "{\"x_mm\":" + Num(Mm(point.x)) + ",\"y_mm\":" + Num(Mm(point.y)) + "}";
    }

    std::string BoundsJson(MCObjectHandle object)
    {
        WorldRect bounds;
        if (!object || !gSDK->GetObjectBounds(object, bounds)) { return "null"; }
        return "{\"left_mm\":" + Num(Mm(bounds.left)) +
               ",\"top_mm\":" + Num(Mm(bounds.top)) +
               ",\"right_mm\":" + Num(Mm(bounds.right)) +
               ",\"bottom_mm\":" + Num(Mm(bounds.bottom)) + "}";
    }

    std::string StyleName(MCObjectHandle wall, InternalIndex& outStyleIndex)
    {
        outStyleIndex = 0;
        if (!gSDK->GetWallStyle(wall, outStyleIndex) || outStyleIndex == 0) { return ""; }
        return ObjectName(gSDK->InternalIndexToHandle(outStyleIndex)).GetCharPtr();
    }

    std::string ComponentJson(MCObjectHandle wall, const VWFC::VWObjects::VWWallObj& wallObj)
    {
        std::ostringstream out;
        out << "[";
        for (size_t i = 0; i < wallObj.GetComponentCount(); ++i)
        {
            if (i > 0) { out << ","; }
            const auto info = wallObj.GetComponentInfo(i);
            TXString className;
            gSDK->ClassIDToName(info.componentClass, className);
            WorldCoord topOffset = 0.0;
            WorldCoord bottomOffset = 0.0;
            Boolean followsTop = false;
            Boolean followsBottom = false;
            const bool hasTopOffset = gSDK->GetComponentOffsetFromWallTop(wall, static_cast<short>(i), topOffset);
            const bool hasBottomOffset = gSDK->GetComponentOffsetFromWallBottom(wall, static_cast<short>(i), bottomOffset);
            const bool hasFollowsTop = gSDK->GetComponentFollowTopWallPeaks(wall, static_cast<short>(i), followsTop);
            const bool hasFollowsBottom = gSDK->GetComponentFollowBottomWallPeaks(wall, static_cast<short>(i), followsBottom);

            out << "{\"index\":" << i
                << ",\"name\":" << JsonString(info.componentName)
                << ",\"class_index\":" << info.componentClass
                << ",\"class_name\":" << JsonString(className)
                << ",\"thickness_mm\":" << Num(Mm(info.width))
                << ",\"top_offset_mm\":" << (hasTopOffset ? Num(Mm(topOffset)) : "null")
                << ",\"bottom_offset_mm\":" << (hasBottomOffset ? Num(Mm(bottomOffset)) : "null")
                << ",\"follows_top_peaks\":" << (hasFollowsTop ? Bool(followsTop) : "null")
                << ",\"follows_bottom_peaks\":" << (hasFollowsBottom ? Bool(followsBottom) : "null")
                << "}";
        }
        out << "]";
        return out.str();
    }

    std::string ElevationProfileJson(VWFC::VWObjects::VWWallObj& wallObj)
    {
        VWFC::Math::VWPolygon2D profile;
        if (!wallObj.GetPolyFromWallElevation(profile, false)) { return "null"; }

        std::ostringstream out;
        out << "[";
        for (size_t i = 0; i < profile.GetVertexCount(); ++i)
        {
            if (i > 0) { out << ","; }
            out << PointJson(profile.GetVertexAt(i));
        }
        out << "]";
        return out.str();
    }

    std::string PeakBreaksJson(const VWFC::VWObjects::VWWallObj& wallObj)
    {
        VWFC::VWObjects::TWallPeakBreaksArray peaks;
        wallObj.EnumerateBreaks(peaks);
        std::ostringstream out;
        out << "[";
        for (size_t i = 0; i < peaks.size(); ++i)
        {
            if (i > 0) { out << ","; }
            out << "{\"index\":" << peaks[i].index
                << ",\"station_mm\":" << Num(Mm(peaks[i].offset))
                << ",\"height_mm\":" << Num(Mm(peaks[i].peakBreak.peakHeight))
                << ",\"top_peak\":" << Bool(peaks[i].peakBreak.topPeak)
                << "}";
        }
        out << "]";
        return out.str();
    }

    std::string ParametricJson(MCObjectHandle object);

    std::string SymbolBreaksJson(const VWFC::VWObjects::VWWallObj& wallObj)
    {
        VWFC::VWObjects::TWallSymbolBreaksArray breaks;
        wallObj.EnumerateBreaks(breaks);
        std::ostringstream out;
        out << "[";
        for (size_t i = 0; i < breaks.size(); ++i)
        {
            if (i > 0) { out << ","; }
            const auto& brk = breaks[i];
            MCObjectHandle insert = brk.symBreak.theSymbol;
            out << "{\"index\":" << brk.index
                << ",\"station_mm\":" << Num(Mm(brk.offset))
                << ",\"height_mm\":" << Num(Mm(brk.symBreak.height))
                << ",\"entity_offset_z_mm\":" << Num(Mm(EntityOffsetZ(insert)))
                << ",\"handle_debug\":" << JsonString(HandleText(insert))
                << ",\"typeN\":" << (insert ? gSDK->GetObjectTypeN(insert) : 0)
                << ",\"name\":" << JsonString(ObjectName(insert))
                << ",\"class_name\":" << JsonString(ClassName(insert))
                << ",\"bounds\":" << BoundsJson(insert)
                << ",\"parametric\":" << ParametricJson(insert)
                << ",\"right_side\":" << Bool(brk.symBreak.rightSide)
                << ",\"flip_h\":" << Bool(brk.symBreak.flipH)
                << ",\"insert_mode\":" << brk.symBreak.insertMode
                << ",\"location_offset_mm\":" << Num(Mm(brk.symBreak.locationOffset))
                << ",\"break_mode\":" << brk.symBreak.breakMode
                << ",\"corner_break\":" << Bool(brk.symBreak.cornerBreak)
                << ",\"span_break\":" << Bool(brk.symBreak.spanBreak)
                << "}";
        }
        out << "]";
        return out.str();
    }

    std::string ExtruderRecordJson(MCObjectHandle object)
    {
        MCObjectHandle record =
            VWFC::VWObjects::VWRecordObj::GetRecordObject(object, kExtruderRecordName);
        if (!record) { return "null"; }

        return "{\"object_name\":" + JsonString(GetRecordText(record, "object_name")) +
               ",\"extrusion_axis_semantic\":" + JsonString(GetRecordText(record, "extrusion_axis_semantic")) +
               ",\"profile_dim_a_semantic\":" + JsonString(GetRecordText(record, "profile_dim_a_semantic")) +
               ",\"profile_dim_b_semantic\":" + JsonString(GetRecordText(record, "profile_dim_b_semantic")) +
               ",\"iqs_length_m\":" + JsonString(GetRecordText(record, "iqs_length_m")) +
               ",\"iqs_width_m\":" + JsonString(GetRecordText(record, "iqs_width_m")) +
               ",\"iqs_height_m\":" + JsonString(GetRecordText(record, "iqs_height_m")) + "}";
    }

    std::string ParametricJson(MCObjectHandle object)
    {
        if (!VWFC::VWObjects::VWParametricObj::IsParametricObject(object)) { return "null"; }

        VWFC::VWObjects::VWParametricObj parametric(object);
        std::ostringstream out;
        out << "{\"name\":" << JsonString(parametric.GetParametricName())
            << ",\"localized_name\":" << JsonString(parametric.GetLocalizedParametricName())
            << ",\"internal_id\":" << parametric.GetInternalID()
            << ",\"parameters\":[";
        for (size_t i = 0; i < parametric.GetParamsCount(); ++i)
        {
            if (i > 0) { out << ","; }
            out << "{\"index\":" << i
                << ",\"name\":" << JsonString(parametric.GetParamName(i))
                << ",\"localized_name\":" << JsonString(parametric.GetParamLocalizedName(i))
                << ",\"style\":" << static_cast<Sint32>(parametric.GetParamStyle(i))
                << ",\"value\":" << JsonString(parametric.GetParamAsString(i))
                << "}";
        }
        out << "]}";
        return out.str();
    }

    std::string BeamReferencesJson()
    {
        std::vector<MCObjectHandle> beams;
        gSDK->ForEachObjectN(allDrawing, [&](MCObjectHandle object) {
            const std::string name = Lower(ObjectName(object));
            if (name.rfind("beam", 0) == 0) { beams.push_back(object); }
        });

        std::ostringstream out;
        out << "[";
        for (size_t i = 0; i < beams.size(); ++i)
        {
            if (i > 0) { out << ","; }
            MCObjectHandle beam = beams[i];
            out << "{\"handle_debug\":" << JsonString(HandleText(beam))
                << ",\"typeN\":" << gSDK->GetObjectTypeN(beam)
                << ",\"name\":" << JsonString(ObjectName(beam))
                << ",\"class_name\":" << JsonString(ClassName(beam))
                << ",\"layer_name\":" << JsonString(LayerName(beam))
                << ",\"bounds\":" << BoundsJson(beam)
                << ",\"iqs_extruder_record\":" << ExtruderRecordJson(beam)
                << ",\"parametric\":" << ParametricJson(beam)
                << "}";
        }
        out << "]";
        return out.str();
    }

    std::string WallJson(MCObjectHandle wall)
    {
        VWFC::VWObjects::VWWallObj wallObj(wall);
        const auto start = wallObj.GetStartPoint();
        const auto end = wallObj.GetEndPoint();
        double startTop = 0.0;
        double startBottom = 0.0;
        double endTop = 0.0;
        double endBottom = 0.0;
        wallObj.GetHeights(startTop, startBottom, endTop, endBottom);
        WorldCoord overallTop = 0.0;
        WorldCoord overallBottom = 0.0;
        gSDK->GetWallOverallHeights(wall, overallTop, overallBottom);
        InternalIndex styleIndex = 0;
        const std::string styleName = StyleName(wall, styleIndex);

        std::ostringstream out;
        out << "{"
            << "\"iqs_uuid\":" << JsonString(EnsureHostUUID(wall))
            << ",\"handle_debug\":" << JsonString(HandleText(wall))
            << ",\"typeN\":" << gSDK->GetObjectTypeN(wall)
            << ",\"name\":" << JsonString(ObjectName(wall))
            << ",\"class_name\":" << JsonString(ClassName(wall))
            << ",\"layer_name\":" << JsonString(LayerName(wall))
            << ",\"is_round\":" << Bool(wallObj.IsRound())
            << ",\"supported_for_v1\":" << Bool(IsSupportedWallForV1(wallObj))
            << ",\"start\":" << PointJson(start)
            << ",\"end\":" << PointJson(end)
            << ",\"length_mm\":" << Num(Mm(Distance(start, end)))
            << ",\"thickness_mm\":" << Num(Mm(wallObj.GetWidth()))
            << ",\"corner_heights_mm\":{\"start_top\":" << Num(Mm(startTop))
            << ",\"start_bottom\":" << Num(Mm(startBottom))
            << ",\"end_top\":" << Num(Mm(endTop))
            << ",\"end_bottom\":" << Num(Mm(endBottom)) << "}"
            << ",\"overall_heights_mm\":{\"top\":" << Num(Mm(overallTop))
            << ",\"bottom\":" << Num(Mm(overallBottom)) << "}"
            << ",\"style_index\":" << styleIndex
            << ",\"style_name\":" << JsonString(styleName)
            << ",\"bounds\":" << BoundsJson(wall)
            << ",\"components\":" << ComponentJson(wall, wallObj)
            << ",\"elevation_profile_vertices\":" << ElevationProfileJson(wallObj)
            << ",\"peak_breaks\":" << PeakBreaksJson(wallObj)
            << ",\"inserted_objects\":" << SymbolBreaksJson(wallObj)
            << "}";
        return out.str();
    }

    void SanitizeSettings(FramingSettings& settings)
    {
        if (settings.componentName.IsEmpty()) { settings.componentName = kRequiredComponentName; }
        if (settings.generatedClassName.IsEmpty()) { settings.generatedClassName = kGeneratedClassName; }
        settings.studWidthMm = std::max(1.0, settings.studWidthMm);
        settings.studDepthMm = std::max(1.0, settings.studDepthMm);
        settings.studSpacingMm = std::max(settings.studWidthMm, settings.studSpacingMm);
        settings.plateHeightMm = std::max(1.0, settings.plateHeightMm);
        settings.plateWidthMm = std::max(1.0, settings.plateWidthMm);
        settings.headerHeightMm = std::max(1.0, settings.headerHeightMm);
        settings.headerWidthMm = std::max(1.0, settings.headerWidthMm);
        settings.noggingHeightMm = std::max(1.0, settings.noggingHeightMm);
        settings.noggingWidthMm = std::max(1.0, settings.noggingWidthMm);
        settings.sillHeightMm = std::max(1.0, settings.sillHeightMm);
        settings.sillWidthMm = std::max(1.0, settings.sillWidthMm);
        settings.noggingCentresMm = std::max(settings.noggingHeightMm, settings.noggingCentresMm);
        settings.noggingStaggerMm = std::max(0.0, settings.noggingStaggerMm);
        settings.bottomPlateCount = std::max<Sint32>(0, settings.bottomPlateCount);
        settings.topPlateCount = std::max<Sint32>(0, settings.topPlateCount);
        settings.kingStudCount = std::max<Sint32>(0, settings.kingStudCount);
        settings.trimmerStudCount = std::max<Sint32>(1, settings.trimmerStudCount);
        if (settings.bottomPlatePrefix.IsEmpty()) { settings.bottomPlatePrefix = "BP"; }
        if (settings.topPlatePrefix.IsEmpty()) { settings.topPlatePrefix = "TP"; }
        if (settings.studPrefix.IsEmpty()) { settings.studPrefix = "S"; }
        if (settings.noggingPrefix.IsEmpty()) { settings.noggingPrefix = "NOG"; }
        if (settings.windowSillPrefix.IsEmpty()) { settings.windowSillPrefix = "WS"; }
        if (settings.lintelPrefix.IsEmpty()) { settings.lintelPrefix = "LIN"; }
        if (settings.doorHeadPrefix.IsEmpty()) { settings.doorHeadPrefix = "DH"; }
    }

    class CFramingSettingsDialog : public VWDialog
    {
    public:
        CFramingSettingsDialog()
            : fTabs(100), fWallPane(101), fMembersPane(102), fOpeningsPane(103),
              fNoggingsPane(104), fAdvancedPane(105),
              fComponentLabel(110), fComponentEdit(111), fClassLabel(112), fClassEdit(113),
              fStudWidthLabel(120), fStudWidthEdit(121), fStudSpacingLabel(122),
              fStudSpacingEdit(123), fPlateHeightLabel(124), fPlateHeightEdit(125),
              fBottomPlateCountLabel(126), fBottomPlateCountEdit(127),
              fTopPlateCountLabel(128), fTopPlateCountEdit(129),
              fHeaderHeightLabel(130), fHeaderHeightEdit(131),
              fHeaderWidthLabel(132), fHeaderWidthEdit(133),
              fDetectDoors(140), fDetectWindows(141),
              fKingStudCountLabel(142), fKingStudCountEdit(143),
              fTrimmerStudCountLabel(144), fTrimmerStudCountEdit(145),
              fGenerateNoggings(150), fNoggingHeightLabel(151), fNoggingHeightEdit(152),
              fNoggingCentresLabel(153), fNoggingCentresEdit(154),
              fNoggingStaggerLabel(155), fNoggingStaggerEdit(156),
              fGenerateCornerStuds(160), fResolveStudOverlaps(161),
              fBottomPlatePrefixLabel(162), fBottomPlatePrefixEdit(163),
              fTopPlatePrefixLabel(164), fTopPlatePrefixEdit(165),
              fStudPrefixLabel(166), fStudPrefixEdit(167),
              fNoggingPrefixLabel(168), fNoggingPrefixEdit(169),
              fWindowSillPrefixLabel(170), fWindowSillPrefixEdit(171),
              fLintelPrefixLabel(172), fLintelPrefixEdit(173),
              fDoorHeadPrefixLabel(174), fDoorHeadPrefixEdit(175),
              fProfileMemberHeader(176), fProfileWidthHeader(177), fProfileHeightHeader(178),
              fStudDepthEdit(179), fPlateWidthEdit(180), fNoggingWidthEdit(181),
              fSillLabel(182), fSillWidthEdit(183), fSillHeightEdit(184)
        {
            fSettings = gSettings;
        }

        bool Run()
        {
            if (this->RunDialogLayout("iQsWallFramerSettings") != kDialogButton_Ok)
            {
                return false;
            }
            SanitizeSettings(fSettings);
            gSettings = fSettings;
            return true;
        }

    protected:
        virtual bool CreateDialogLayout() override
        {
            if (!this->CreateDialog("iQs Wall Framer Settings", "Generate", "Cancel", false))
            {
                return false;
            }

            if (!fTabs.CreateControl(this) ||
                !fWallPane.CreateControl(this, "Wall") ||
                !fMembersPane.CreateControl(this, "Members") ||
                !fOpeningsPane.CreateControl(this, "Openings") ||
                !fNoggingsPane.CreateControl(this, "Noggings") ||
                !fAdvancedPane.CreateControl(this, "Advanced"))
            {
                return false;
            }

            if (!fComponentLabel.CreateControl(this, "Framing component name:", 28) ||
                !fComponentEdit.CreateControl(this, fSettings.componentName, 28) ||
                !fClassLabel.CreateControl(this, "Generated framing class:", 28) ||
                !fClassEdit.CreateControl(this, fSettings.generatedClassName, 28))
            {
                return false;
            }

            if (!fProfileMemberHeader.CreateControl(this, "Member", 18) ||
                !fProfileWidthHeader.CreateControl(this, "Width", 14) ||
                !fProfileHeightHeader.CreateControl(this, "Height", 14) ||
                !fStudWidthLabel.CreateControl(this, "Stud:", 18) ||
                !fStudWidthEdit.CreateControl(this, fSettings.studWidthMm, 14, VWEditRealCtrl::kEditControlDimension) ||
                !fStudDepthEdit.CreateControl(this, fSettings.studDepthMm, 14, VWEditRealCtrl::kEditControlDimension) ||
                !fStudSpacingLabel.CreateControl(this, "Stud centres:", 28) ||
                !fStudSpacingEdit.CreateControl(this, fSettings.studSpacingMm, 14, VWEditRealCtrl::kEditControlDimension) ||
                !fPlateHeightLabel.CreateControl(this, "Plate:", 18) ||
                !fPlateWidthEdit.CreateControl(this, fSettings.plateWidthMm, 14, VWEditRealCtrl::kEditControlDimension) ||
                !fPlateHeightEdit.CreateControl(this, fSettings.plateHeightMm, 14, VWEditRealCtrl::kEditControlDimension) ||
                !fBottomPlateCountLabel.CreateControl(this, "Bottom plate count:", 28) ||
                !fBottomPlateCountEdit.CreateControl(this, fSettings.bottomPlateCount, 14) ||
                !fTopPlateCountLabel.CreateControl(this, "Top plate count:", 28) ||
                !fTopPlateCountEdit.CreateControl(this, fSettings.topPlateCount, 14) ||
                !fHeaderHeightLabel.CreateControl(this, "Lintel:", 18) ||
                !fHeaderHeightEdit.CreateControl(this, fSettings.headerHeightMm, 14, VWEditRealCtrl::kEditControlDimension) ||
                !fHeaderWidthEdit.CreateControl(this, fSettings.headerWidthMm, 14, VWEditRealCtrl::kEditControlDimension) ||
                !fNoggingHeightLabel.CreateControl(this, "Nogging:", 18) ||
                !fNoggingWidthEdit.CreateControl(this, fSettings.noggingWidthMm, 14, VWEditRealCtrl::kEditControlDimension) ||
                !fNoggingHeightEdit.CreateControl(this, fSettings.noggingHeightMm, 14, VWEditRealCtrl::kEditControlDimension) ||
                !fSillLabel.CreateControl(this, "Window sill:", 18) ||
                !fSillWidthEdit.CreateControl(this, fSettings.sillWidthMm, 14, VWEditRealCtrl::kEditControlDimension) ||
                !fSillHeightEdit.CreateControl(this, fSettings.sillHeightMm, 14, VWEditRealCtrl::kEditControlDimension))
            {
                return false;
            }

            if (!fDetectDoors.CreateControl(this, "Frame door openings") ||
                !fDetectWindows.CreateControl(this, "Frame window openings") ||
                !fKingStudCountLabel.CreateControl(this, "King studs per jamb:", 28) ||
                !fKingStudCountEdit.CreateControl(this, fSettings.kingStudCount, 14) ||
                !fTrimmerStudCountLabel.CreateControl(this, "Trimmer studs per jamb:", 28) ||
                !fTrimmerStudCountEdit.CreateControl(this, fSettings.trimmerStudCount, 14) ||
                !fGenerateNoggings.CreateControl(this, "Generate noggings") ||
                !fNoggingCentresLabel.CreateControl(this, "Nogging centres:", 28) ||
                !fNoggingCentresEdit.CreateControl(this, fSettings.noggingCentresMm, 14, VWEditRealCtrl::kEditControlDimension) ||
                !fNoggingStaggerLabel.CreateControl(this, "Nogging stagger:", 28) ||
                !fNoggingStaggerEdit.CreateControl(this, fSettings.noggingStaggerMm, 14, VWEditRealCtrl::kEditControlDimension))
            {
                return false;
            }

            if (!fGenerateCornerStuds.CreateControl(this, "Generate lining-fixing corner studs") ||
                !fResolveStudOverlaps.CreateControl(this, "Suppress overlapping studs") ||
                !fBottomPlatePrefixLabel.CreateControl(this, "Bottom plate prefix:", 22) ||
                !fBottomPlatePrefixEdit.CreateControl(this, fSettings.bottomPlatePrefix, 12) ||
                !fTopPlatePrefixLabel.CreateControl(this, "Top plate prefix:", 22) ||
                !fTopPlatePrefixEdit.CreateControl(this, fSettings.topPlatePrefix, 12) ||
                !fStudPrefixLabel.CreateControl(this, "Stud prefix:", 22) ||
                !fStudPrefixEdit.CreateControl(this, fSettings.studPrefix, 12) ||
                !fNoggingPrefixLabel.CreateControl(this, "Nogging prefix:", 22) ||
                !fNoggingPrefixEdit.CreateControl(this, fSettings.noggingPrefix, 12) ||
                !fWindowSillPrefixLabel.CreateControl(this, "Window sill prefix:", 22) ||
                !fWindowSillPrefixEdit.CreateControl(this, fSettings.windowSillPrefix, 12) ||
                !fLintelPrefixLabel.CreateControl(this, "Lintel prefix:", 22) ||
                !fLintelPrefixEdit.CreateControl(this, fSettings.lintelPrefix, 12) ||
                !fDoorHeadPrefixLabel.CreateControl(this, "Door head prefix:", 22) ||
                !fDoorHeadPrefixEdit.CreateControl(this, fSettings.doorHeadPrefix, 12))
            {
                return false;
            }

            this->AddFirstGroupControl(&fTabs);
            fTabs.AddPane(&fWallPane);
            fTabs.AddPane(&fMembersPane);
            fTabs.AddPane(&fOpeningsPane);
            fTabs.AddPane(&fNoggingsPane);
            fTabs.AddPane(&fAdvancedPane);

            fWallPane.AddFirstGroupControl(&fComponentLabel);
            this->AddRightControl(&fComponentLabel, &fComponentEdit);
            this->AddBelowControl(&fComponentLabel, &fClassLabel);
            this->AddRightControl(&fClassLabel, &fClassEdit);

            fMembersPane.AddFirstGroupControl(&fProfileMemberHeader);
            this->AddRightControl(&fProfileMemberHeader, &fProfileWidthHeader);
            this->AddRightControl(&fProfileWidthHeader, &fProfileHeightHeader);
            AddProfileRow(fStudWidthLabel, fStudWidthEdit, fStudDepthEdit, &fProfileMemberHeader);
            AddProfileRow(fPlateHeightLabel, fPlateWidthEdit, fPlateHeightEdit, &fStudWidthLabel);
            AddProfileRow(fNoggingHeightLabel, fNoggingWidthEdit, fNoggingHeightEdit, &fPlateHeightLabel);
            AddProfileRow(fSillLabel, fSillWidthEdit, fSillHeightEdit, &fNoggingHeightLabel);
            AddProfileRow(fHeaderHeightLabel, fHeaderWidthEdit, fHeaderHeightEdit, &fSillLabel);
            this->AddBelowControl(&fHeaderHeightLabel, &fStudSpacingLabel);
            this->AddRightControl(&fStudSpacingLabel, &fStudSpacingEdit);
            AddLabelledControl(fBottomPlateCountLabel, fBottomPlateCountEdit, &fStudSpacingLabel);
            AddLabelledControl(fTopPlateCountLabel, fTopPlateCountEdit, &fBottomPlateCountLabel);

            fOpeningsPane.AddFirstGroupControl(&fDetectDoors);
            this->AddBelowControl(&fDetectDoors, &fDetectWindows);
            this->AddBelowControl(&fDetectWindows, &fKingStudCountLabel);
            this->AddRightControl(&fKingStudCountLabel, &fKingStudCountEdit);
            AddLabelledControl(fTrimmerStudCountLabel, fTrimmerStudCountEdit, &fKingStudCountLabel);

            fNoggingsPane.AddFirstGroupControl(&fGenerateNoggings);
            this->AddBelowControl(&fGenerateNoggings, &fNoggingCentresLabel);
            this->AddRightControl(&fNoggingCentresLabel, &fNoggingCentresEdit);
            AddLabelledControl(fNoggingStaggerLabel, fNoggingStaggerEdit, &fNoggingCentresLabel);

            fAdvancedPane.AddFirstGroupControl(&fGenerateCornerStuds);
            this->AddBelowControl(&fGenerateCornerStuds, &fResolveStudOverlaps);
            this->AddBelowControl(&fResolveStudOverlaps, &fBottomPlatePrefixLabel);
            this->AddRightControl(&fBottomPlatePrefixLabel, &fBottomPlatePrefixEdit);
            AddLabelledControl(fTopPlatePrefixLabel, fTopPlatePrefixEdit, &fBottomPlatePrefixLabel);
            AddLabelledControl(fStudPrefixLabel, fStudPrefixEdit, &fTopPlatePrefixLabel);
            AddLabelledControl(fNoggingPrefixLabel, fNoggingPrefixEdit, &fStudPrefixLabel);
            AddLabelledControl(fWindowSillPrefixLabel, fWindowSillPrefixEdit, &fNoggingPrefixLabel);
            AddLabelledControl(fLintelPrefixLabel, fLintelPrefixEdit, &fWindowSillPrefixLabel);
            AddLabelledControl(fDoorHeadPrefixLabel, fDoorHeadPrefixEdit, &fLintelPrefixLabel);
            return true;
        }

        virtual void OnDDXInitialize() override
        {
            this->AddDDX_EditText(111, &fSettings.componentName);
            this->AddDDX_EditText(113, &fSettings.generatedClassName);
            this->AddDDX_EditReal(121, &fSettings.studWidthMm, VWEditRealCtrl::kEditControlDimension);
            this->AddDDX_EditReal(179, &fSettings.studDepthMm, VWEditRealCtrl::kEditControlDimension);
            this->AddDDX_EditReal(123, &fSettings.studSpacingMm, VWEditRealCtrl::kEditControlDimension);
            this->AddDDX_EditReal(125, &fSettings.plateHeightMm, VWEditRealCtrl::kEditControlDimension);
            this->AddDDX_EditReal(180, &fSettings.plateWidthMm, VWEditRealCtrl::kEditControlDimension);
            this->AddDDX_EditInteger(127, &fSettings.bottomPlateCount);
            this->AddDDX_EditInteger(129, &fSettings.topPlateCount);
            this->AddDDX_EditReal(131, &fSettings.headerHeightMm, VWEditRealCtrl::kEditControlDimension);
            this->AddDDX_EditReal(133, &fSettings.headerWidthMm, VWEditRealCtrl::kEditControlDimension);
            this->AddDDX_CheckButton(140, &fSettings.detectDoors);
            this->AddDDX_CheckButton(141, &fSettings.detectWindows);
            this->AddDDX_EditInteger(143, &fSettings.kingStudCount);
            this->AddDDX_EditInteger(145, &fSettings.trimmerStudCount);
            this->AddDDX_CheckButton(150, &fSettings.generateNoggings);
            this->AddDDX_EditReal(152, &fSettings.noggingHeightMm, VWEditRealCtrl::kEditControlDimension);
            this->AddDDX_EditReal(181, &fSettings.noggingWidthMm, VWEditRealCtrl::kEditControlDimension);
            this->AddDDX_EditReal(183, &fSettings.sillWidthMm, VWEditRealCtrl::kEditControlDimension);
            this->AddDDX_EditReal(184, &fSettings.sillHeightMm, VWEditRealCtrl::kEditControlDimension);
            this->AddDDX_EditReal(154, &fSettings.noggingCentresMm, VWEditRealCtrl::kEditControlDimension);
            this->AddDDX_EditReal(156, &fSettings.noggingStaggerMm, VWEditRealCtrl::kEditControlDimension);
            this->AddDDX_CheckButton(160, &fSettings.generateCornerStuds);
            this->AddDDX_CheckButton(161, &fSettings.resolveStudOverlaps);
            this->AddDDX_EditText(163, &fSettings.bottomPlatePrefix);
            this->AddDDX_EditText(165, &fSettings.topPlatePrefix);
            this->AddDDX_EditText(167, &fSettings.studPrefix);
            this->AddDDX_EditText(169, &fSettings.noggingPrefix);
            this->AddDDX_EditText(171, &fSettings.windowSillPrefix);
            this->AddDDX_EditText(173, &fSettings.lintelPrefix);
            this->AddDDX_EditText(175, &fSettings.doorHeadPrefix);
        }

    private:
        template<class TControl>
        void AddLabelledControl(VWStaticTextCtrl& label, TControl& control,
                                VWStaticTextCtrl* previousLabel)
        {
            if (previousLabel) { this->AddBelowControl(previousLabel, &label); }
            this->AddRightControl(&label, &control);
        }

        void AddProfileRow(VWStaticTextCtrl& label, VWEditRealCtrl& width,
                           VWEditRealCtrl& height, VWStaticTextCtrl* previousLabel)
        {
            this->AddBelowControl(previousLabel, &label);
            this->AddRightControl(&label, &width);
            this->AddRightControl(&width, &height);
        }

        FramingSettings fSettings;
        VWTabCtrl fTabs;
        VWTabPaneCtrl fWallPane;
        VWTabPaneCtrl fMembersPane;
        VWTabPaneCtrl fOpeningsPane;
        VWTabPaneCtrl fNoggingsPane;
        VWTabPaneCtrl fAdvancedPane;
        VWStaticTextCtrl fComponentLabel;
        VWEditTextCtrl fComponentEdit;
        VWStaticTextCtrl fClassLabel;
        VWEditTextCtrl fClassEdit;
        VWStaticTextCtrl fStudWidthLabel;
        VWEditRealCtrl fStudWidthEdit;
        VWStaticTextCtrl fStudSpacingLabel;
        VWEditRealCtrl fStudSpacingEdit;
        VWStaticTextCtrl fPlateHeightLabel;
        VWEditRealCtrl fPlateHeightEdit;
        VWStaticTextCtrl fBottomPlateCountLabel;
        VWEditIntegerCtrl fBottomPlateCountEdit;
        VWStaticTextCtrl fTopPlateCountLabel;
        VWEditIntegerCtrl fTopPlateCountEdit;
        VWStaticTextCtrl fHeaderHeightLabel;
        VWEditRealCtrl fHeaderHeightEdit;
        VWStaticTextCtrl fHeaderWidthLabel;
        VWEditRealCtrl fHeaderWidthEdit;
        VWCheckButtonCtrl fDetectDoors;
        VWCheckButtonCtrl fDetectWindows;
        VWStaticTextCtrl fKingStudCountLabel;
        VWEditIntegerCtrl fKingStudCountEdit;
        VWStaticTextCtrl fTrimmerStudCountLabel;
        VWEditIntegerCtrl fTrimmerStudCountEdit;
        VWCheckButtonCtrl fGenerateNoggings;
        VWStaticTextCtrl fNoggingHeightLabel;
        VWEditRealCtrl fNoggingHeightEdit;
        VWStaticTextCtrl fNoggingCentresLabel;
        VWEditRealCtrl fNoggingCentresEdit;
        VWStaticTextCtrl fNoggingStaggerLabel;
        VWEditRealCtrl fNoggingStaggerEdit;
        VWCheckButtonCtrl fGenerateCornerStuds;
        VWCheckButtonCtrl fResolveStudOverlaps;
        VWStaticTextCtrl fBottomPlatePrefixLabel;
        VWEditTextCtrl fBottomPlatePrefixEdit;
        VWStaticTextCtrl fTopPlatePrefixLabel;
        VWEditTextCtrl fTopPlatePrefixEdit;
        VWStaticTextCtrl fStudPrefixLabel;
        VWEditTextCtrl fStudPrefixEdit;
        VWStaticTextCtrl fNoggingPrefixLabel;
        VWEditTextCtrl fNoggingPrefixEdit;
        VWStaticTextCtrl fWindowSillPrefixLabel;
        VWEditTextCtrl fWindowSillPrefixEdit;
        VWStaticTextCtrl fLintelPrefixLabel;
        VWEditTextCtrl fLintelPrefixEdit;
        VWStaticTextCtrl fDoorHeadPrefixLabel;
        VWEditTextCtrl fDoorHeadPrefixEdit;
        VWStaticTextCtrl fProfileMemberHeader;
        VWStaticTextCtrl fProfileWidthHeader;
        VWStaticTextCtrl fProfileHeightHeader;
        VWEditRealCtrl fStudDepthEdit;
        VWEditRealCtrl fPlateWidthEdit;
        VWEditRealCtrl fNoggingWidthEdit;
        VWStaticTextCtrl fSillLabel;
        VWEditRealCtrl fSillWidthEdit;
        VWEditRealCtrl fSillHeightEdit;
    };
}

class CMenuSink : public VCOMImpl<VectorWorks::Extension::IMenuEventSink>
{
public:
    CMenuSink(IVWUnknown* parent)
        : VCOMImpl<VectorWorks::Extension::IMenuEventSink>(parent)
    {
    }

    virtual Sint32 VCOM_CALLTYPE Execute(MenuMessage* message) override
    {
        if (!message || message->fAction != MenuDoInterfaceMessage::kAction) { return 0; }

        std::vector<MCObjectHandle> walls;
        size_t ignoredSelectedObjects = 0;
        gSDK->ForEachObjectN(allSelected, [&](MCObjectHandle object) {
            if (VWFC::VWObjects::VWWallObj::IsWallObject(object)) { walls.push_back(object); }
            else { ++ignoredSelectedObjects; }
        });

        if (walls.empty())
        {
            gSDK->AlertInform("Select one or more Vectorworks walls, then run the probe again.",
                              "", false, "iQs Wall Framer", "No walls selected");
            return 0;
        }

        std::vector<MCObjectHandle> contextWalls;
        gSDK->ForEachObjectN(allDrawing, [&](MCObjectHandle object) {
            if (VWFC::VWObjects::VWWallObj::IsWallObject(object))
            {
                contextWalls.push_back(object);
            }
        });

        CFramingSettingsDialog settingsDialog;
        if (!settingsDialog.Run()) { return 0; }

        EnsureUniqueSelectedHostUUIDs(walls);

        const std::string timestamp = Timestamp();
        const auto outPath = ProbeOutputDir() / ("wall_framing_probe_" + timestamp + ".json");
        std::ofstream out(outPath);
        if (!out)
        {
            gSDK->AlertInform("Could not create the wall probe JSON file.",
                              "", false, "iQs Wall Framer", "Probe failed");
            return 0;
        }
        out << "{\"schema\":" << JsonString(std::string(kSchema))
            << ",\"schema_version\":\"0.1\""
            << ",\"export_unix\":" << Now()
            << ",\"units\":\"mm\""
            << ",\"selected_wall_count\":" << walls.size()
            << ",\"ignored_selected_object_count\":" << ignoredSelectedObjects
            << ",\"beam_references\":" << BeamReferencesJson()
            << ",\"walls\":[";
        for (size_t i = 0; i < walls.size(); ++i)
        {
            if (i > 0) { out << ","; }
            out << WallJson(walls[i]);
        }
        out << "]}\n";
        out.close();

        std::vector<GeneratedFrame> frames;
        size_t refreshedFrameCount = 0;
        size_t duplicateHostCount = 0;
        size_t duplicateFrameCount = 0;
        size_t unsupportedWallCount = 0;
        size_t missingComponentCount = 0;
        const auto generateFrames = [&](bool reportCleanup) {
            frames.clear();
            for (MCObjectHandle wall : walls)
            {
                VWFC::VWObjects::VWWallObj wallObj(wall);
                if (!IsSupportedWallForV1(wallObj))
                {
                    if (reportCleanup) { ++unsupportedWallCount; }
                    continue;
                }
                if (!FindFramingComponent(wall, wallObj).found)
                {
                    if (reportCleanup) { ++missingComponentCount; }
                    continue;
                }

                const TXString hostUUID = EnsureHostUUID(wall);
                const std::vector<MCObjectHandle> linkedFrames = FindLinkedFrames(hostUUID);
                if (reportCleanup && linkedFrames.size() > 1)
                {
                    ++duplicateHostCount;
                    duplicateFrameCount += linkedFrames.size();
                }
                for (MCObjectHandle linkedFrame : linkedFrames)
                {
                    gSDK->DeleteObject(linkedFrame, true);
                    if (reportCleanup) { ++refreshedFrameCount; }
                }

                GeneratedFrame frame = GenerateSimpleFrame(
                    wall, FindPlateExtents(wall, contextWalls));
                if (frame.group) { frames.push_back(frame); }
            }
        };

        generateFrames(true);
        generateFrames(false);
        gSDK->RefreshRenderingForSelectedObjects();

        const auto generationPath =
            ProbeOutputDir() / ("stud_wall_frame_generation_" + timestamp + ".json");
        std::ofstream generationOut(generationPath);
        if (!generationOut)
        {
            gSDK->AlertInform("Frame geometry was created, but the generation JSON file could not be written.",
                              "", false, "iQs Wall Framer", "Generation export failed");
            return 0;
        }
        generationOut << "{\"schema\":" << JsonString(std::string(kGenerationSchema))
                      << ",\"schema_version\":\"0.1\""
                      << ",\"export_unix\":" << Now()
                      << ",\"units\":\"mm\""
                      << ",\"generated_frame_count\":" << frames.size()
                      << ",\"refreshed_frame_count\":" << refreshedFrameCount
                      << ",\"duplicate_host_count\":" << duplicateHostCount
                      << ",\"duplicate_frame_count\":" << duplicateFrameCount
                      << ",\"unsupported_wall_count\":" << unsupportedWallCount
                      << ",\"missing_component_count\":" << missingComponentCount
                      << ",\"frames\":[";
        for (size_t i = 0; i < frames.size(); ++i)
        {
            if (i > 0) { generationOut << ","; }
            generationOut << GeneratedFrameJson(frames[i]);
        }
        generationOut << "]}\n";
        generationOut.close();

        TXString completionMessage;
        completionMessage.Format("Probed %d selected wall(s) and generated %d simple frame(s). Removed %d previous frame group(s) before regeneration.\n\nSkipped %d unsupported wall(s). V0.1 supports straight walls with linear start-to-end top and bottom rakes only.\n\nSkipped %d wall(s) without the required '%s' component.\n\nDuplicate cleanup: %d host wall(s) had %d linked frame groups. Those stale groups were removed before regeneration.\n\nGeneration JSON written to:\n%s",
                       static_cast<Sint32>(walls.size()), static_cast<Sint32>(frames.size()),
                       static_cast<Sint32>(refreshedFrameCount),
                       static_cast<Sint32>(unsupportedWallCount),
                       static_cast<Sint32>(missingComponentCount), gSettings.componentName.GetCharPtr(),
                       static_cast<Sint32>(duplicateHostCount), static_cast<Sint32>(duplicateFrameCount),
                       generationPath.string().c_str());
        gSDK->AlertInform(completionMessage, "", false, "iQs Wall Framer", "Frame generation complete");
        return 0;
    }
};

const char* DefaultPluginVWRIdentifier()
{
    return kPluginVWRIdentifier;
}

extern "C" Sint32 GS_EXTERNAL_ENTRY plugin_module_ver()
{
    return SDK_VERSION;
}

namespace iQsWallFramer
{
    static VWFC::PluginSupport::SMenuDef_Legacy MakeMenuDef()
    {
        VWFC::PluginSupport::SMenuDef_Legacy def{};
        def.fNeeds             = static_cast<decltype(def.fNeeds)>(0);
        def.fNeedsNot          = static_cast<decltype(def.fNeedsNot)>(0);
        def.fTitle.fListID     = 0;
        def.fTitle.fIndex      = 0;
        def.fCategory.fListID  = 0;
        def.fCategory.fIndex   = 0;
        def.fHelpText.fID      = 0;
        def.fVersionCreated    = 0;
        def.fVersionModified   = 0;
        def.fVersionRetired    = 0;
        def.fOverrideHelpID    = nullptr;
        return def;
    }

    static VWFC::PluginSupport::SMenuDef_Legacy kMenuDef = MakeMenuDef();

    class CExtMenu : public VWFC::PluginSupport::VWExtensionMenu
    {
    public:
        static const VWIID& _GetIID()
        {
            static VWIID iid = { 0x9549e87d, 0x05cf, 0x4aba,
                                  { 0x91, 0x4d, 0x64, 0xc9, 0xb7, 0xde, 0xc1, 0xb2 } };
            return iid;
        }

        CExtMenu(CallBackPtr cbp)
            : VWFC::PluginSupport::VWExtensionMenu(cbp, kMenuDef)
        {
        }

        virtual VectorWorks::Extension::IMenuEventSink*
        CreateMenuEventSink(IVWUnknown* parent) override
        {
            return new CMenuSink(parent);
        }

        virtual Sint32 VCOM_CALLTYPE GetVersion() override { return 1; }

        virtual const TXString& VCOM_CALLTYPE GetUniversalName() override
        {
            static TXString name = kMenuUniversalName;
            return name;
        }
    };
}

extern "C" Sint32 GS_EXTERNAL_ENTRY plugin_module_main(
    Sint32 action, void* moduleInfo, const VWIID& iid,
    IVWUnknown*& inOutInterface, CallBackPtr cbp)
{
    ::GS_InitializeVCOM(cbp);
    Sint32 reply = 0;
    using namespace VWFC::PluginSupport;
    REGISTER_Extension<iQsWallFramer::CExtMenu>(
        GROUPID_ExtensionMenu, action, moduleInfo, iid, inOutInterface, cbp, reply);
    return reply;
}
