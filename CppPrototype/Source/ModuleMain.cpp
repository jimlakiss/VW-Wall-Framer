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
    constexpr const char* kOpeningRecordName   = "iQs_Opening";
    constexpr const char* kFrameRecordName     = "iQs_StudWallFrame";
    constexpr const char* kExtruderRecordName  = "iQs Extruder V0.1";
    constexpr const char* kGeneratedClassName  = "Wall-Timber Frame";
    constexpr const char* kRequiredComponentName = "Timber Frame";
    constexpr const char* kSchema              = "iqs_wall_framing_probe_v0_1";
    constexpr const char* kGenerationSchema    = "iqs_stud_wall_frame_generation_v0_1";
    constexpr double kStudWidthMm              = 45.0;
    constexpr double kStudSpacingMm            = 450.0;
    constexpr double kPlateHeightMm            = 45.0;
    constexpr double kHeaderHeightMm           = 45.0;
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
        double ledgerTriggerHeightMm = 120.0;
        double noggingCentresMm = kNoggingCentresMm;
        double noggingStaggerMm = kNoggingHeightMm;
        Sint32 bottomPlateCount = static_cast<Sint32>(kBottomPlateCount);
        Sint32 topPlateCount = static_cast<Sint32>(kTopPlateCount);
        bool detectDoors = true;
        bool detectWindows = true;
        bool generateLedgers = true;
        bool generateUpperLedgers = false;
        bool continueJackStudsToLintelUnderside = false;
        Sint32 jambStudCount = 1;
        Sint32 trimmerStudCount = 1;
        bool generateNoggings = true;
        bool generateCornerStuds = true;
        bool resolveStudOverlaps = true;
        TXString bottomPlatePrefix = "BP";
        TXString topPlatePrefix = "TP";
        TXString studPrefix = "S";
        TXString endStudPrefix = "ES";
        TXString cornerStudPrefix = "CS";
        TXString jambStudPrefix = "JAM";
        TXString trimmerStudPrefix = "TR";
        TXString jackStudPrefix = "JS";
        TXString noggingPrefix = "NOG";
        TXString windowSillPrefix = "WS";
        TXString lintelPrefix = "LIN";
        TXString doorHeadPrefix = "DH";
        TXString ledgerPrefix = "LED";
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

    std::string WholeMm(double value)
    {
        return std::to_string(static_cast<long long>(std::llround(value)));
    }

    double PositiveDoubleOrDefault(const TXString& value, double fallback)
    {
        if (value.IsEmpty()) { return fallback; }
        try
        {
            return std::max(1.0, std::stod(value.GetCharPtr()));
        }
        catch (...)
        {
            return fallback;
        }
    }

    Sint32 PositiveIntegerOrDefault(const TXString& value, Sint32 fallback)
    {
        if (value.IsEmpty()) { return fallback; }
        try
        {
            return std::max<Sint32>(1, static_cast<Sint32>(std::stoi(value.GetCharPtr())));
        }
        catch (...)
        {
            return fallback;
        }
    }

    bool BoolOrDefault(const TXString& value, bool fallback)
    {
        if (value.IsEmpty()) { return fallback; }
        return value.EqualNoCase("true") || value.EqualNoCase("yes") || value == "1";
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

    void EnsureOpeningRecordFormat()
    {
        TFormatHandler format(kOpeningRecordName);
        EnsureTextField(format, "iqs_uuid");
        EnsureTextField(format, "source_type", "VW_WALL_OPENING_PIO");
        EnsureTextField(format, "lintel_id");
        EnsureTextField(format, "lintel_count", "1");
        EnsureTextField(format, "lintel_width_mm");
        EnsureTextField(format, "lintel_height_mm");
        EnsureTextField(format, "lower_ledger");
        EnsureTextField(format, "upper_ledger");
        EnsureTextField(format, "continue_jack_studs_to_lintel_underside");
        EnsureTextField(format, "sill_support_jacks");
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

    MCObjectHandle EnsureOpeningRecord(MCObjectHandle opening)
    {
        EnsureOpeningRecordFormat();
        MCObjectHandle record =
            VWFC::VWObjects::VWRecordObj::GetRecordObject(opening, kOpeningRecordName);
        if (!record)
        {
            TFormatHandler format(kOpeningRecordName);
            record = format.AttachRecordToObject(opening);
        }

        TXString uuid = GetRecordText(record, "iqs_uuid");
        if (uuid.IsEmpty())
        {
            uuid = NewUUID().c_str();
            SetRecordText(record, "iqs_uuid", uuid);
        }
        SetRecordText(record, "source_type", "VW_WALL_OPENING_PIO");
        return record;
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

    InternalIndex GeneratedFrameGroupClassID()
    {
        const TXString className = gSettings.generatedClassName + "-Frame group";
        const InternalIndex classID = gSDK->AddClass(className);
        gSDK->SetClassVisibility(classID, true);
        return classID;
    }

    struct ActiveClassGuard
    {
        ActiveClassGuard()
            : classID(gSDK->GetActiveClass())
        {
        }

        ~ActiveClassGuard()
        {
            if (classID != 0)
            {
                VWFC::VWObjects::VWClass(classID).SetThisClassAsDefault();
            }
        }

        InternalIndex classID = 0;
    };

    std::string MemberClassDescription(const std::string& memberType)
    {
        if (memberType == "BOTTOM_PLATE") { return "Bottom plate"; }
        if (memberType == "TOP_PLATE") { return "Top plate"; }
        if (memberType == "END_STUD") { return "End stud"; }
        if (memberType == "CORNER_STUD") { return "Corner stud"; }
        if (memberType == "JAMB_STUD") { return "Jamb stud"; }
        if (memberType == "TRIMMER_STUD") { return "Trimmer stud"; }
        if (memberType == "JACK_STUD") { return "Jack stud"; }
        if (memberType == "LINTEL" || memberType == "HEADER") { return "Lintel"; }
        if (memberType == "SILL") { return "Window sill"; }
        if (memberType == "LEDGER") { return "Window and door ledger"; }
        if (memberType == "NOGGING") { return "Nogging"; }
        return "Stud";
    }

    RGBColor MemberClassColor(const std::string& memberType)
    {
        if (memberType == "BOTTOM_PLATE") { return { 46260, 38550, 28270 }; }
        if (memberType == "TOP_PLATE") { return { 53970, 46260, 33410 }; }
        if (memberType == "END_STUD") { return { 34695, 48830, 59110 }; }
        if (memberType == "CORNER_STUD") { return { 26728, 41120, 52685 }; }
        if (memberType == "JAMB_STUD") { return { 45232, 35980, 55512 }; }
        if (memberType == "TRIMMER_STUD") { return { 55512, 41120, 58082 }; }
        if (memberType == "JACK_STUD") { return { 59110, 48830, 33410 }; }
        if (memberType == "LINTEL" || memberType == "HEADER") { return { 51400, 20560, 20560 }; }
        if (memberType == "SILL") { return { 25700, 51400, 38550 }; }
        if (memberType == "LEDGER") { return { 20560, 46260, 51400 }; }
        if (memberType == "NOGGING") { return { 41120, 51400, 30840 }; }
        return { 51400, 43690, 33410 };
    }

    bool ClassExists(const TXString& className)
    {
        const InternalIndex maxClassID = gSDK->MaxClassID();
        for (InternalIndex classID = 1; classID <= maxClassID; ++classID)
        {
            TXString existingName;
            gSDK->ClassIDToName(classID, existingName);
            if (existingName.EqualNoCase(className)) { return true; }
        }
        return false;
    }

    InternalIndex GeneratedMemberClassID(const std::string& memberType)
    {
        const TXString className =
            gSettings.generatedClassName + "-" + MemberClassDescription(memberType).c_str();
        const bool existed = ClassExists(className);
        const InternalIndex classID = gSDK->AddClass(className);
        if (!existed || !gSDK->GetClUseGraphic(classID))
        {
            const RGBColor rgb = MemberClassColor(memberType);
            ColorRef color = 0;
            gSDK->RGBToColorIndex(rgb, color);
            gSDK->SetClColor(classID, { color, color, color, color });
            gSDK->SetClFillPat(classID, 1);
            gSDK->SetClUseGraphic(classID, true);
        }
        gSDK->SetClassVisibility(classID, true);
        return classID;
    }

    const TXString& MemberPrefix(const std::string& memberType)
    {
        if (memberType == "END_STUD") { return gSettings.endStudPrefix; }
        if (memberType == "CORNER_STUD") { return gSettings.cornerStudPrefix; }
        if (memberType == "JAMB_STUD") { return gSettings.jambStudPrefix; }
        if (memberType == "TRIMMER_STUD") { return gSettings.trimmerStudPrefix; }
        if (memberType == "JACK_STUD") { return gSettings.jackStudPrefix; }
        if (memberType == "LEDGER") { return gSettings.ledgerPrefix; }
        return gSettings.studPrefix;
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
        MCObjectHandle handle = nullptr;
        std::string key;
        std::string userID;
        std::string type;
        double stationMm = 0.0;
        double widthMm = 0.0;
        double bottomMm = 0.0;
        double topMm = 0.0;
        double lintelWidthMm = 0.0;
        double lintelHeightMm = 0.0;
        Sint32 lintelCount = 1;
        std::string lintelID;
        bool lowerLedger = false;
        bool upperLedger = false;
        bool continueJackStudsToLintelUnderside = false;
        bool sillSupportJacks = false;
    };

    struct OpeningLintelOverride
    {
        double widthMm = 0.0;
        double heightMm = 0.0;
        Sint32 count = 1;
        std::string lintelID;
        bool lowerLedger = false;
        bool upperLedger = false;
        bool continueJackStudsToLintelUnderside = false;
        bool sillSupportJacks = false;
    };

    std::map<std::string, OpeningLintelOverride> gOpeningLintelOverrides;

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

    double SlopedMemberVerticalHeight(double memberHeightMm, double slope)
    {
        return memberHeightMm * std::sqrt(1.0 + slope * slope);
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

    std::vector<FramingComponent> FindFramingComponents(
        MCObjectHandle wall, const VWFC::VWObjects::VWWallObj& wallObj)
    {
        std::vector<FramingComponent> results;
        double runningOffset = -wallObj.GetWidth() / 2.0;
        for (size_t i = 0; i < wallObj.GetComponentCount(); ++i)
        {
            const auto info = wallObj.GetComponentInfo(i);
            TXString className;
            gSDK->ClassIDToName(info.componentClass, className);
            const double centerOffset = -(runningOffset + info.width / 2.0);
            if (Lower(info.componentName) == Lower(gSettings.componentName))
            {
                FramingComponent result;
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
                results.push_back(result);
            }
            runningOffset += info.width;
        }

        return results;
    }

    FramingComponent FindFramingComponent(MCObjectHandle wall,
                                          const VWFC::VWObjects::VWWallObj& wallObj)
    {
        const std::vector<FramingComponent> components = FindFramingComponents(wall, wallObj);
        return components.empty() ? FramingComponent{} : components.front();
    }

    TXString ComponentSourceRef(const FramingComponent& component, bool includeIndex)
    {
        if (!includeIndex) { return component.name; }
        TXString result = component.name;
        result += " #";
        result += TXString::ToStringInt(static_cast<Sint32>(component.index + 1));
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

    bool LineIntersectionStations(const VWFC::Math::VWPoint2D& origin,
                                  const VWFC::Math::VWPoint2D& direction,
                                  const VWFC::Math::VWPoint2D& otherOrigin,
                                  const VWFC::Math::VWPoint2D& otherDirection,
                                  double& station,
                                  double& otherStation)
    {
        const double cross =
            direction.x * otherDirection.y - direction.y * otherDirection.x;
        if (std::abs(cross) <= 0.000001) { return false; }
        const double dx = otherOrigin.x - origin.x;
        const double dy = otherOrigin.y - origin.y;
        station = (dx * otherDirection.y - dy * otherDirection.x) / cross;
        otherStation = (dx * direction.y - dy * direction.x) / cross;
        return true;
    }

    double Dot(const VWFC::Math::VWPoint2D& lhs, const VWFC::Math::VWPoint2D& rhs)
    {
        return lhs.x * rhs.x + lhs.y * rhs.y;
    }

    PlateExtents FindPlateExtents(MCObjectHandle wall,
                                  const std::vector<MCObjectHandle>& contextWalls,
                                  const FramingComponent& component)
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
            const std::vector<FramingComponent> otherComponents =
                FindFramingComponents(contextWalls[otherIndex], otherObj);
            for (const FramingComponent& otherComponent : otherComponents)
            {
                const double halfOtherDepth = otherComponent.depthMm / 2.0;
                const bool startSharesEndpoint =
                    SamePoint(start, otherStart) || SamePoint(start, otherEnd);
                const bool endSharesEndpoint =
                    SamePoint(end, otherStart) || SamePoint(end, otherEnd);
                if (startSharesEndpoint || endSharesEndpoint)
                {
                    const bool thisWallRunsThroughCorner = wallIndex < otherIndex;
                    if (startSharesEndpoint)
                    {
                        result.startStationMm =
                            thisWallRunsThroughCorner
                                ? std::min(result.startStationMm, -halfOtherDepth)
                                : std::max(result.startStationMm, halfOtherDepth);
                    }
                    if (endSharesEndpoint)
                    {
                        result.endStationMm =
                            thisWallRunsThroughCorner
                                ? std::max(result.endStationMm, wallLength + halfOtherDepth)
                                : std::min(result.endStationMm, wallLength - halfOtherDepth);
                    }
                    continue;
                }

                const auto otherComponentStart =
                    WallPoint(otherStart, otherAlong, otherNormal, 0.0,
                              otherComponent.centerOffsetMm);
                auto endpointInsideOtherComponentRun = [&](const VWFC::Math::VWPoint2D& point) {
                    const VWFC::Math::VWPoint2D delta(point.x - otherComponentStart.x,
                                                     point.y - otherComponentStart.y);
                    const double otherStation = Dot(delta, otherAlong);
                    const double otherOffset = Dot(delta, otherNormal);
                    return otherStation > 1.0 && otherStation < otherLength - 1.0 &&
                           std::abs(otherOffset) <= halfOtherDepth + 1.0;
                };

                const auto componentEnd =
                    WallPoint(start, along, normal, wallLength, component.centerOffsetMm);
                const bool trimStart =
                    endpointInsideOtherComponentRun(componentStart);
                const bool trimEnd =
                    endpointInsideOtherComponentRun(componentEnd);
                if (!trimStart && !trimEnd) { continue; }

                for (double otherFaceOffset :
                     { otherComponent.centerOffsetMm - halfOtherDepth,
                       otherComponent.centerOffsetMm + halfOtherDepth })
                {
                    const auto otherFaceStart =
                        WallPoint(otherStart, otherAlong, otherNormal, 0.0, otherFaceOffset);

                    double faceStation = 0.0;
                    double otherFaceStation = 0.0;
                    if (!LineIntersectionStations(componentStart, along, otherFaceStart,
                                                  otherAlong, faceStation, otherFaceStation))
                    {
                        continue;
                    }

                    const bool hitsOtherRun =
                        otherFaceStation > 1.0 && otherFaceStation < otherLength - 1.0;
                    if (!hitsOtherRun) { continue; }

                    if (trimStart &&
                        faceStation >= -1.0 &&
                        faceStation <= otherComponent.depthMm + 1.0)
                    {
                        result.startStationMm =
                            std::max(result.startStationMm, faceStation);
                    }
                    if (trimEnd &&
                        faceStation <= wallLength + 1.0 &&
                        faceStation >= wallLength - otherComponent.depthMm - 1.0)
                    {
                        result.endStationMm =
                            std::min(result.endStationMm, faceStation);
                    }
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
                                        double zStartMm, double zHeightMm,
                                        const std::string& memberType)
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
        gSDK->SetObjectClass(handle, GeneratedMemberClassID(memberType));
        gSDK->SetFColorsByClass(handle);
        gSDK->SetFPatByClass(handle);
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
                                        double plateHeightMm, const std::string& memberType)
    {
        if (alongLengthMm <= 0.0 || depthMm <= 0.0 || plateHeightMm <= 0.0) { return nullptr; }

        const double halfDepth = depthMm / 2.0;
        const double slope = (axisEndZMm - axisStartZMm) / alongLengthMm;
        const double halfVerticalHeight =
            SlopedMemberVerticalHeight(plateHeightMm, slope) / 2.0;
        const auto startFace =
            WallPoint(wallStart, along, normal, stationStartMm, centerOffsetMm - halfDepth);
        const auto endFace =
            WallPoint(wallStart, along, normal, stationStartMm + alongLengthMm,
                      centerOffsetMm - halfDepth);

        VWFC::Math::VWPolygon3D profile;
        profile.AddVertex(
            VWFC::Math::VWPoint3D(startFace.x, startFace.y, axisStartZMm - halfVerticalHeight));
        profile.AddVertex(
            VWFC::Math::VWPoint3D(startFace.x, startFace.y, axisStartZMm + halfVerticalHeight));
        profile.AddVertex(
            VWFC::Math::VWPoint3D(endFace.x, endFace.y, axisEndZMm + halfVerticalHeight));
        profile.AddVertex(
            VWFC::Math::VWPoint3D(endFace.x, endFace.y, axisEndZMm - halfVerticalHeight));
        profile.SetClosed(true);

        const VWFC::Math::VWPoint3D extrusionDir(normal.x * depthMm, normal.y * depthMm, 0.0);
        VWFC::VWObjects::VWSolidObj member(profile, extrusionDir);
        MCObjectHandle handle = member;
        group.AddObject(handle);
        gSDK->SetObjectClass(handle, GeneratedMemberClassID(memberType));
        gSDK->SetFColorsByClass(handle);
        gSDK->SetFPatByClass(handle);
        gSDK->ResetObject(handle);
        return handle;
    }

    MCObjectHandle AddVerticalMember(VWFC::VWObjects::VWGroupObj& group,
                                     const VWFC::Math::VWPoint2D& wallStart,
                                     const VWFC::Math::VWPoint2D& along,
                                     const VWFC::Math::VWPoint2D& normal,
                                     double stationMm, double memberWidthMm,
                                     double centerOffsetMm, double depthMm,
                                     double centrelineBottomMm, double centrelineTopMm,
                                     double bottomSlope, double topSlope,
                                     const std::string& memberType)
    {
        if (memberWidthMm <= 0.0 || depthMm <= 0.0 ||
            centrelineTopMm <= centrelineBottomMm)
        {
            return nullptr;
        }

        const double halfWidth = memberWidthMm / 2.0;
        const double halfDepth = depthMm / 2.0;
        const auto leftFace =
            WallPoint(wallStart, along, normal, stationMm - halfWidth,
                      centerOffsetMm - halfDepth);
        const auto rightFace =
            WallPoint(wallStart, along, normal, stationMm + halfWidth,
                      centerOffsetMm - halfDepth);

        VWFC::Math::VWPolygon3D profile;
        profile.AddVertex(VWFC::Math::VWPoint3D(
            leftFace.x, leftFace.y, centrelineBottomMm - bottomSlope * halfWidth));
        profile.AddVertex(VWFC::Math::VWPoint3D(
            leftFace.x, leftFace.y, centrelineTopMm - topSlope * halfWidth));
        profile.AddVertex(VWFC::Math::VWPoint3D(
            rightFace.x, rightFace.y, centrelineTopMm + topSlope * halfWidth));
        profile.AddVertex(VWFC::Math::VWPoint3D(
            rightFace.x, rightFace.y, centrelineBottomMm + bottomSlope * halfWidth));
        profile.SetClosed(true);

        const VWFC::Math::VWPoint3D extrusionDir(normal.x * depthMm, normal.y * depthMm, 0.0);
        VWFC::VWObjects::VWSolidObj member(profile, extrusionDir);
        MCObjectHandle handle = member;
        group.AddObject(handle);
        gSDK->SetObjectClass(handle, GeneratedMemberClassID(memberType));
        gSDK->SetFColorsByClass(handle);
        gSDK->SetFPatByClass(handle);
        gSDK->ResetObject(handle);
        return handle;
    }

    MCObjectHandle AddHorizontalMember(VWFC::VWObjects::VWGroupObj& group,
                                       const VWFC::Math::VWPoint2D& wallStart,
                                       const VWFC::Math::VWPoint2D& along,
                                       const VWFC::Math::VWPoint2D& normal,
                                       double stationStartMm, double alongLengthMm,
                                       double centerOffsetMm, double depthMm,
                                       double zStartMm, double zHeightMm,
                                       const std::string& memberType)
    {
        if (alongLengthMm <= 0.0 || depthMm <= 0.0 || zHeightMm <= 0.0) { return nullptr; }

        const double halfDepth = depthMm / 2.0;
        const auto startFace =
            WallPoint(wallStart, along, normal, stationStartMm, centerOffsetMm - halfDepth);
        const auto endFace =
            WallPoint(wallStart, along, normal, stationStartMm + alongLengthMm,
                      centerOffsetMm - halfDepth);

        VWFC::Math::VWPolygon3D profile;
        profile.AddVertex(VWFC::Math::VWPoint3D(startFace.x, startFace.y, zStartMm));
        profile.AddVertex(VWFC::Math::VWPoint3D(startFace.x, startFace.y, zStartMm + zHeightMm));
        profile.AddVertex(VWFC::Math::VWPoint3D(endFace.x, endFace.y, zStartMm + zHeightMm));
        profile.AddVertex(VWFC::Math::VWPoint3D(endFace.x, endFace.y, zStartMm));
        profile.SetClosed(true);

        const VWFC::Math::VWPoint3D extrusionDir(normal.x * depthMm, normal.y * depthMm, 0.0);
        VWFC::VWObjects::VWSolidObj member(profile, extrusionDir);
        MCObjectHandle handle = member;
        group.AddObject(handle);
        gSDK->SetObjectClass(handle, GeneratedMemberClassID(memberType));
        gSDK->SetFColorsByClass(handle);
        gSDK->SetFPatByClass(handle);
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
            opening.handle = insert;
            MCObjectHandle openingRecord = EnsureOpeningRecord(insert);
            opening.key = GetRecordText(openingRecord, "iqs_uuid").GetCharPtr();
            opening.userID =
                std::string(parametric.GetParamString("IDPrefix").GetCharPtr()) +
                std::string(parametric.GetParamString("IDLabel").GetCharPtr()) +
                std::string(parametric.GetParamString("IDSuffix").GetCharPtr());
            opening.stationMm = brk.offset;
            const double frameBottom =
                Interpolate(frameBottomStart, frameBottomEnd, opening.stationMm, wallLength);
            const double insertOffsetZ = EntityOffsetZ(insert);
            const double openingBaseZ = std::abs(insertOffsetZ) > 0.001
                                            ? insertOffsetZ
                                            : frameBottom;
            if (parametricName.EqualNoCase("Door"))
            {
                if (!gSettings.detectDoors) { continue; }
                opening.type = "DOOR";
                opening.widthMm = parametric.GetParamReal("ROWidth");
                opening.bottomMm = openingBaseZ;
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
                    opening.topMm = openingBaseZ + elevationMm;
                    opening.bottomMm = opening.topMm - heightMm;
                }
                else
                {
                    opening.bottomMm = openingBaseZ + elevationMm;
                    opening.topMm = opening.bottomMm + heightMm;
                }
            }
            else
            {
                continue;
            }

            if (opening.widthMm <= 0.0 || opening.topMm <= opening.bottomMm) { continue; }
            const auto overrideIt = gOpeningLintelOverrides.find(opening.key);
            const TXString storedWidth = GetRecordText(openingRecord, "lintel_width_mm");
            const TXString storedHeight = GetRecordText(openingRecord, "lintel_height_mm");
            const TXString storedCount = GetRecordText(openingRecord, "lintel_count");
            const TXString storedID = GetRecordText(openingRecord, "lintel_id");
            const TXString storedLowerLedger = GetRecordText(openingRecord, "lower_ledger");
            const TXString storedUpperLedger = GetRecordText(openingRecord, "upper_ledger");
            const TXString storedJackStudOverrun =
                GetRecordText(openingRecord, "continue_jack_studs_to_lintel_underside");
            const TXString storedSillSupportJacks =
                GetRecordText(openingRecord, "sill_support_jacks");
            const double persistedWidth =
                PositiveDoubleOrDefault(storedWidth, gSettings.headerWidthMm);
            const double persistedHeight =
                PositiveDoubleOrDefault(storedHeight, gSettings.headerHeightMm);
            const Sint32 persistedCount = PositiveIntegerOrDefault(storedCount, 1);
            opening.lintelWidthMm =
                overrideIt != gOpeningLintelOverrides.end()
                    ? overrideIt->second.widthMm
                    : persistedWidth;
            opening.lintelHeightMm =
                overrideIt != gOpeningLintelOverrides.end()
                    ? overrideIt->second.heightMm
                    : persistedHeight;
            opening.lintelCount =
                overrideIt != gOpeningLintelOverrides.end()
                    ? overrideIt->second.count
                    : persistedCount;
            opening.lintelID =
                overrideIt != gOpeningLintelOverrides.end()
                    ? overrideIt->second.lintelID
                    : storedID.GetCharPtr();
            opening.lowerLedger =
                overrideIt != gOpeningLintelOverrides.end()
                    ? overrideIt->second.lowerLedger
                    : BoolOrDefault(storedLowerLedger, gSettings.generateLedgers);
            opening.upperLedger =
                overrideIt != gOpeningLintelOverrides.end()
                    ? overrideIt->second.upperLedger
                    : BoolOrDefault(storedUpperLedger, gSettings.generateUpperLedgers);
            opening.continueJackStudsToLintelUnderside =
                overrideIt != gOpeningLintelOverrides.end()
                    ? overrideIt->second.continueJackStudsToLintelUnderside
                    : BoolOrDefault(storedJackStudOverrun,
                                    gSettings.continueJackStudsToLintelUnderside);
            opening.sillSupportJacks =
                overrideIt != gOpeningLintelOverrides.end()
                    ? overrideIt->second.sillSupportJacks
                    : BoolOrDefault(storedSillSupportJacks, false);
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

    GeneratedFrame GenerateSimpleFrame(
        MCObjectHandle wall, const FramingComponent& component,
        const PlateExtents& plateExtents, bool includeComponentIndex,
        std::map<std::string, size_t>& memberNameCounters)
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

        ActiveClassGuard activeClassGuard;
        VWFC::VWObjects::VWGroupObj group;
        result.group = group;
        result.frameUUID = NewUUID().c_str();
        result.hostUUID = EnsureHostUUID(wall);
        result.sourceComponentMode = "COMPONENT_NAME";
        result.sourceComponentRef = ComponentSourceRef(component, includeComponentIndex);
        result.wallLengthMm = wallLength;
        result.wallHeightMm = frameHeight;
        result.frameDepthMm = component.depthMm;

        const std::string frameName = "iQs_StudWallFrame_" + std::string(result.frameUUID.GetCharPtr());
        gSDK->SetObjectName(result.group, frameName.c_str());
        gSDK->SetObjectClass(result.group, GeneratedFrameGroupClassID());

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
                             const TXString& profileDimBSemantic,
                             double centerOffsetMm =
                                 std::numeric_limits<double>::quiet_NaN()) {
            MCObjectHandle memberHandle =
                AddHorizontalMember(group, start, along, normal, stationStart, alongLength,
                                    std::isnan(centerOffsetMm)
                                        ? component.centerOffsetMm
                                        : centerOffsetMm,
                                    geometryDepth, zStart, zHeight,
                                    type);
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
                                  double stationStart, double stationEnd,
                                  double axisStartZ, double axisEndZ) {
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
            const double slope =
                (trimmedAxisEndZ - trimmedAxisStartZ) / alongLength;
            const double grossLength =
                std::sqrt(alongLength * alongLength +
                          (std::abs(trimmedAxisEndZ - trimmedAxisStartZ) +
                           SlopedMemberVerticalHeight(kPlateHeightMm, slope)) *
                              (std::abs(trimmedAxisEndZ - trimmedAxisStartZ) +
                               SlopedMemberVerticalHeight(kPlateHeightMm, slope)));
            MCObjectHandle memberHandle =
                AddSlopedPlateMember(group, start, along, normal,
                                     stationStart, alongLength,
                                     component.centerOffsetMm, gSettings.plateWidthMm,
                                     trimmedAxisStartZ, trimmedAxisEndZ,
                                     kPlateHeightMm, type);
            if (!memberHandle) { return; }

            FrameMember member{ id, type, grossLength, kPlateHeightMm, gSettings.plateWidthMm,
                                stationStart, trimmedAxisStartZ - kPlateHeightMm / 2.0,
                                trimmedAxisEndZ + kPlateHeightMm / 2.0,
                                grossLength, gSettings.plateWidthMm, kPlateHeightMm,
                                "Length", "Height", "Width" };
            result.members.push_back(member);
            TagExtruderSemanticMember(memberHandle, member);
        };

        auto addVerticalMember = [&](const std::string& id, const std::string& type,
                                     double station, double centrelineBottom,
                                     double centrelineTop, double memberBottomSlope,
                                     double memberTopSlope) {
            const double centrelineLength = centrelineTop - centrelineBottom;
            if (centrelineLength <= 0.0) { return; }
            MCObjectHandle memberHandle =
                AddVerticalMember(group, start, along, normal, station, kStudWidthMm,
                                  component.centerOffsetMm, gSettings.studDepthMm,
                                  centrelineBottom, centrelineTop,
                                  memberBottomSlope, memberTopSlope, type);
            if (!memberHandle) { return; }

            const double grossBottom =
                GrossVerticalBottom(centrelineBottom, kStudWidthMm, memberBottomSlope);
            const double grossLength =
                GrossVerticalLength(centrelineLength, kStudWidthMm,
                                    memberBottomSlope, memberTopSlope);
            FrameMember member{ id, type, grossLength, kStudWidthMm, gSettings.studDepthMm,
                                station, grossBottom, grossBottom + grossLength,
                                grossLength, kStudWidthMm, gSettings.studDepthMm,
                                "Length", "Width", "Height" };
            result.members.push_back(member);
            TagExtruderSemanticMember(memberHandle, member);
        };

        for (size_t i = 0; i < kBottomPlateCount; ++i)
        {
            const double offset =
                (static_cast<double>(i) + 0.5) *
                SlopedMemberVerticalHeight(kPlateHeightMm, bottomSlope);
            std::vector<std::pair<double, double>> excludedIntervals;
            for (const WallOpening& opening : openings)
            {
                const double plateBottom =
                    Interpolate(wallProfile.bottomStartMm, wallProfile.bottomEndMm,
                                opening.stationMm, wallLength) +
                    static_cast<double>(i) *
                        SlopedMemberVerticalHeight(kPlateHeightMm, bottomSlope);
                const double plateTop =
                    plateBottom + SlopedMemberVerticalHeight(kPlateHeightMm, bottomSlope);
                if (opening.bottomMm >= plateTop - 0.001 ||
                    opening.topMm <= plateBottom + 0.001)
                {
                    continue;
                }

                const double openingLeft =
                    std::max(plateExtents.startStationMm,
                             opening.stationMm - opening.widthMm / 2.0);
                const double openingRight =
                    std::min(plateExtents.endStationMm,
                             opening.stationMm + opening.widthMm / 2.0);
                if (openingRight > openingLeft + 0.001)
                {
                    excludedIntervals.emplace_back(openingLeft, openingRight);
                }
            }

            std::sort(excludedIntervals.begin(), excludedIntervals.end());
            std::vector<std::pair<double, double>> mergedExcludedIntervals;
            for (const auto& interval : excludedIntervals)
            {
                if (mergedExcludedIntervals.empty() ||
                    interval.first > mergedExcludedIntervals.back().second + 0.001)
                {
                    mergedExcludedIntervals.push_back(interval);
                }
                else
                {
                    mergedExcludedIntervals.back().second =
                        std::max(mergedExcludedIntervals.back().second, interval.second);
                }
            }

            double segmentStart = plateExtents.startStationMm;
            for (const auto& interval : mergedExcludedIntervals)
            {
                if (interval.first > segmentStart + 0.001)
                {
                    addSlopedPlate(nextMemberName(gSettings.bottomPlatePrefix), "BOTTOM_PLATE",
                                   segmentStart, interval.first,
                                   wallProfile.bottomStartMm + offset,
                                   wallProfile.bottomEndMm + offset);
                }
                segmentStart = std::max(segmentStart, interval.second);
            }
            if (plateExtents.endStationMm > segmentStart + 0.001)
            {
                addSlopedPlate(nextMemberName(gSettings.bottomPlatePrefix), "BOTTOM_PLATE",
                               segmentStart, plateExtents.endStationMm,
                               wallProfile.bottomStartMm + offset,
                               wallProfile.bottomEndMm + offset);
            }
        }
        for (size_t i = 0; i < kTopPlateCount; ++i)
        {
            const double offset =
                (static_cast<double>(i) + 0.5) *
                SlopedMemberVerticalHeight(kPlateHeightMm, topSlope);
            addSlopedPlate(nextMemberName(gSettings.topPlatePrefix), "TOP_PLATE",
                           plateExtents.startStationMm, plateExtents.endStationMm,
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
            double centrelineBottomMm = 0.0;
            double centrelineTopMm = 0.0;
            double bottomSlope = 0.0;
            double topSlope = 0.0;
            int priority = 0;
        };

        struct VerticalSegment
        {
            double bottomMm = 0.0;
            double topMm = 0.0;
        };

        auto lowerLedgerHeightForOpening = [&](const WallOpening& opening) {
            return opening.lowerLedger &&
                           opening.lintelHeightMm > gSettings.ledgerTriggerHeightMm
                       ? kStudWidthMm
                       : 0.0;
        };
        auto upperLedgerHeightForOpening = [&](const WallOpening& opening) {
            return opening.upperLedger &&
                           opening.lintelHeightMm > gSettings.ledgerTriggerHeightMm
                       ? kStudWidthMm
                       : 0.0;
        };
        auto lintelBottomForOpening = [&](const WallOpening& opening) {
            return opening.topMm + lowerLedgerHeightForOpening(opening);
        };
        auto lintelTopForOpening = [&](const WallOpening& opening) {
            return lintelBottomForOpening(opening) + opening.lintelHeightMm;
        };
        auto lintelAssemblyTopForOpening = [&](const WallOpening& opening) {
            return lintelTopForOpening(opening) + upperLedgerHeightForOpening(opening);
        };
        auto verticalBottomAboveLintel = [&](const WallOpening& opening) {
            return upperLedgerHeightForOpening(opening) > 0.0
                       ? lintelAssemblyTopForOpening(opening)
                       : opening.continueJackStudsToLintelUnderside ||
                           opening.lintelHeightMm < kStudWidthMm / 2.0
                             ? lintelBottomForOpening(opening)
                             : lintelTopForOpening(opening);
        };

        auto splitVerticalRangeForOpenings =
            [&](double station, double memberWidth, double bottom, double top,
                size_t ignoredOpeningIndex) {
                std::vector<VerticalSegment> segments;
                if (top <= bottom) { return segments; }
                segments.push_back({ bottom, top });

                const double memberLeft = station - memberWidth / 2.0;
                const double memberRight = station + memberWidth / 2.0;
                if (ignoredOpeningIndex < openings.size())
                {
                    const WallOpening& ownOpening = openings[ignoredOpeningIndex];
                    const double ownOpeningCentre =
                        (ownOpening.bottomMm + ownOpening.topMm) / 2.0;
                    for (size_t openingIndex = 0; openingIndex < openings.size(); ++openingIndex)
                    {
                        if (openingIndex == ignoredOpeningIndex) { continue; }

                        const WallOpening& opening = openings[openingIndex];
                        const double openingLeft = opening.stationMm - opening.widthMm / 2.0;
                        const double openingRight = opening.stationMm + opening.widthMm / 2.0;
                        if (memberLeft >= openingRight - 0.001 ||
                            memberRight <= openingLeft + 0.001)
                        {
                            continue;
                        }

                        const double openingCentre = (opening.bottomMm + opening.topMm) / 2.0;
                        if (openingCentre > ownOpeningCentre)
                        {
                            const double sillUnderside =
                                opening.bottomMm -
                                (opening.type == "WINDOW" ? gSettings.sillHeightMm : 0.0);
                            segments[0].topMm = std::min(segments[0].topMm, sillUnderside);
                        }
                        else
                        {
                            segments[0].bottomMm =
                                std::max(segments[0].bottomMm,
                                         verticalBottomAboveLintel(opening));
                        }
                    }

                    if (segments[0].topMm <= segments[0].bottomMm + 0.001)
                    {
                        segments.clear();
                    }
                    return segments;
                }

                for (size_t openingIndex = 0; openingIndex < openings.size(); ++openingIndex)
                {
                    const WallOpening& opening = openings[openingIndex];
                    const double openingLeft = opening.stationMm - opening.widthMm / 2.0;
                    const double openingRight = opening.stationMm + opening.widthMm / 2.0;
                    if (memberLeft >= openingRight - 0.001 ||
                        memberRight <= openingLeft + 0.001)
                    {
                        continue;
                    }

                    std::vector<VerticalSegment> remaining;
                    const double framedOpeningBottom =
                        opening.bottomMm -
                        (opening.type == "WINDOW" ? gSettings.sillHeightMm : 0.0);
                    const double framedOpeningTop = verticalBottomAboveLintel(opening);
                    for (const VerticalSegment& segment : segments)
                    {
                        if (segment.topMm <= framedOpeningBottom + 0.001 ||
                            segment.bottomMm >= framedOpeningTop - 0.001)
                        {
                            remaining.push_back(segment);
                            continue;
                        }
                        if (segment.bottomMm < framedOpeningBottom - 0.001)
                        {
                            remaining.push_back({ segment.bottomMm, framedOpeningBottom });
                        }
                        if (segment.topMm > framedOpeningTop + 0.001)
                        {
                            remaining.push_back({ framedOpeningTop, segment.topMm });
                        }
                    }
                    segments = remaining;
                }
                return segments;
            };

        std::vector<PendingVerticalMember> pendingVerticalMembers;
        auto queueVerticalMember = [&](const std::string& id, const std::string& type,
                                       double station, double centrelineBottom,
                                       double centrelineTop, double memberBottomSlope,
                                       double memberTopSlope, int priority,
                                       size_t ignoredOpeningIndex =
                                           std::numeric_limits<size_t>::max()) {
            if (centrelineTop <= centrelineBottom) { return; }
            for (const VerticalSegment& segment :
                 splitVerticalRangeForOpenings(station, kStudWidthMm, centrelineBottom,
                                               centrelineTop, ignoredOpeningIndex))
            {
                const double segmentBottomSlope =
                    std::abs(segment.bottomMm - centrelineBottom) < 0.001
                        ? memberBottomSlope
                        : 0.0;
                const double segmentTopSlope =
                    std::abs(segment.topMm - centrelineTop) < 0.001
                        ? memberTopSlope
                        : 0.0;
                const double grossBottom =
                    GrossVerticalBottom(segment.bottomMm, kStudWidthMm, segmentBottomSlope);
                const double grossLength =
                    GrossVerticalLength(segment.topMm - segment.bottomMm, kStudWidthMm,
                                        segmentBottomSlope, segmentTopSlope);
                pendingVerticalMembers.push_back(
                    { id, type, station, grossBottom, grossLength, grossLength,
                      segment.bottomMm, segment.topMm,
                      segmentBottomSlope, segmentTopSlope, priority });
            }
        };

        struct DirectJackSegment
        {
            double stationMm = 0.0;
            double bottomMm = 0.0;
            double topMm = 0.0;
        };
        std::vector<DirectJackSegment> directJackSegments;
        auto addSegmentedJack = [&](double station, double centrelineBottom,
                                    double centrelineTop, double memberBottomSlope,
                                    double memberTopSlope,
                                    size_t ignoredOpeningIndex) {
            for (const VerticalSegment& segment :
                 splitVerticalRangeForOpenings(station, kStudWidthMm, centrelineBottom,
                                               centrelineTop, ignoredOpeningIndex))
            {
                const bool overlapsExisting =
                    std::any_of(directJackSegments.begin(), directJackSegments.end(),
                                [&](const DirectJackSegment& existing) {
                                    return std::abs(existing.stationMm - station) <
                                               kStudWidthMm - 0.001 &&
                                           segment.bottomMm < existing.topMm - 0.001 &&
                                           segment.topMm > existing.bottomMm + 0.001;
                                });
                if (gSettings.resolveStudOverlaps && overlapsExisting) { continue; }

                addVerticalMember(
                    nextMemberName(MemberPrefix("JACK_STUD")), "JACK_STUD", station,
                    segment.bottomMm, segment.topMm,
                    std::abs(segment.bottomMm - centrelineBottom) < 0.001
                        ? memberBottomSlope
                        : 0.0,
                    std::abs(segment.topMm - centrelineTop) < 0.001
                        ? memberTopSlope
                        : 0.0);
                directJackSegments.push_back({ station, segment.bottomMm, segment.topMm });
            }
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
                static_cast<double>(kBottomPlateCount) *
                    SlopedMemberVerticalHeight(kPlateHeightMm, bottomSlope);
            const double studTop =
                Interpolate(wallProfile.topStartMm, wallProfile.topEndMm,
                            stations[i], wallLength) -
                static_cast<double>(kTopPlateCount) *
                    SlopedMemberVerticalHeight(kPlateHeightMm, topSlope);
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
                studBottom, studTop, bottomSlope, topSlope,
                endStud ? 100 : 50);
        }

        for (double station : plateExtents.cornerStudStationsMm)
        {
            const double studBottom =
                Interpolate(wallProfile.bottomStartMm, wallProfile.bottomEndMm,
                            station, wallLength) +
                static_cast<double>(kBottomPlateCount) *
                    SlopedMemberVerticalHeight(kPlateHeightMm, bottomSlope);
            const double studTop =
                Interpolate(wallProfile.topStartMm, wallProfile.topEndMm,
                            station, wallLength) -
                static_cast<double>(kTopPlateCount) *
                    SlopedMemberVerticalHeight(kPlateHeightMm, topSlope);
            const double clearStudHeight = studTop - studBottom;
            if (clearStudHeight <= 0.0) { continue; }

            queueVerticalMember(
                "", "CORNER_STUD", station,
                studBottom, studTop, bottomSlope, topSlope, 60);
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
                    static_cast<double>(kBottomPlateCount) *
                        SlopedMemberVerticalHeight(kPlateHeightMm, bottomSlope);
                const double availableTop =
                    Interpolate(wallProfile.topStartMm, wallProfile.topEndMm,
                                scheduleStation, wallLength) -
                    static_cast<double>(kTopPlateCount) *
                        SlopedMemberVerticalHeight(kPlateHeightMm, topSlope);
                const double studTop = std::min(requestedTop, availableTop);
                const double height = studTop - studBottom;
                if (height <= 0.0) { return; }
                queueVerticalMember(
                    "", type, scheduleStation,
                    studBottom, studTop, bottomSlope, studTopSlope,
                    type == "TRIMMER_STUD" ? 80 : 70, i);
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
            for (Sint32 i = 0; i < gSettings.jambStudCount; ++i)
            {
                const double jambOffset =
                    (static_cast<double>(gSettings.trimmerStudCount + i) + 0.5) * kStudWidthMm;
                addOpeningStud("JAMB_STUD",
                               openingLeft - jambOffset - kStudWidthMm / 2.0,
                               openingLeft - jambOffset,
                               std::numeric_limits<double>::max(), topSlope);
                addOpeningStud("JAMB_STUD",
                               openingRight + jambOffset - kStudWidthMm / 2.0,
                               openingRight + jambOffset,
                               std::numeric_limits<double>::max(), topSlope);
            }

            const double trimmerPackWidth =
                static_cast<double>(gSettings.trimmerStudCount) * kStudWidthMm;
            const double headerStart = openingLeft - trimmerPackWidth;
            const double headerLength = opening.widthMm + 2.0 * trimmerPackWidth;
            const double lowerLedgerHeight = lowerLedgerHeightForOpening(opening);
            if (lowerLedgerHeight > 0.0)
            {
                addMember(nextMemberName(MemberPrefix("LEDGER")), "LEDGER",
                          headerStart, headerLength,
                          opening.topMm, lowerLedgerHeight, gSettings.studDepthMm,
                          opening.stationMm,
                          headerLength, gSettings.studDepthMm, lowerLedgerHeight,
                          "Height", "Length", "Width");
            }
            const TXString lintelPrefix =
                opening.lintelID.empty()
                    ? (opening.type == "WINDOW"
                           ? gSettings.lintelPrefix
                           : gSettings.doorHeadPrefix)
                    : TXString(opening.lintelID.c_str());
            for (Sint32 lintelIndex = 0; lintelIndex < opening.lintelCount; ++lintelIndex)
            {
                const double centerOffset =
                    component.centerOffsetMm +
                    (static_cast<double>(lintelIndex) -
                     (static_cast<double>(opening.lintelCount) - 1.0) / 2.0) *
                        opening.lintelWidthMm;
                addMember(opening.lintelID.empty()
                              ? nextMemberName(lintelPrefix)
                              : opening.lintelID,
                          "LINTEL",
                          headerStart, headerLength,
                          lintelBottomForOpening(opening), opening.lintelHeightMm,
                          opening.lintelWidthMm, opening.stationMm,
                          headerLength, opening.lintelWidthMm, opening.lintelHeightMm,
                          "Height", "Length", "Width", centerOffset);
            }
            const double upperLedgerHeight = upperLedgerHeightForOpening(opening);
            if (upperLedgerHeight > 0.0)
            {
                addMember(nextMemberName(MemberPrefix("LEDGER")), "LEDGER",
                          headerStart, headerLength,
                          lintelTopForOpening(opening), upperLedgerHeight,
                          gSettings.studDepthMm, opening.stationMm,
                          headerLength, gSettings.studDepthMm, upperLedgerHeight,
                          "Height", "Length", "Width");
            }

            if (opening.type == "WINDOW")
            {
                addMember(nextMemberName(gSettings.windowSillPrefix), "SILL", openingLeft, opening.widthMm,
                          opening.bottomMm - gSettings.sillHeightMm, gSettings.sillHeightMm,
                          gSettings.sillWidthMm,
                          opening.stationMm,
                          opening.widthMm, gSettings.sillWidthMm, gSettings.sillHeightMm,
                          "Width", "Length", "Height");
            }

            const double upperJackBottom = verticalBottomAboveLintel(opening);
            const double lowerJackTop = opening.bottomMm - gSettings.sillHeightMm;
            std::vector<double> lowerJackStations;
            auto addLowerJack = [&](double station) {
                const bool overlapsExisting =
                    std::any_of(lowerJackStations.begin(), lowerJackStations.end(),
                                [&](double existingStation) {
                                    return std::abs(existingStation - station) <
                                           kStudWidthMm - 0.001;
                                });
                if (overlapsExisting) { return; }

                const double localLowerJackBottom =
                    Interpolate(wallProfile.bottomStartMm, wallProfile.bottomEndMm,
                                station, wallLength) +
                    static_cast<double>(kBottomPlateCount) *
                        SlopedMemberVerticalHeight(kPlateHeightMm, bottomSlope);
                if (lowerJackTop <= localLowerJackBottom) { return; }

                const double height = lowerJackTop - localLowerJackBottom;
                addSegmentedJack(
                    station, localLowerJackBottom, lowerJackTop,
                    bottomSlope, 0.0, i);
                lowerJackStations.push_back(station);
            };

            std::vector<double> jackStations;
            if (opening.type == "WINDOW" && opening.sillSupportJacks)
            {
                addLowerJack(openingLeft + kStudWidthMm / 2.0);
                addLowerJack(openingRight - kStudWidthMm / 2.0);
            }
            for (double station : stations)
            {
                if (station <= openingLeft || station >= openingRight)
                {
                    continue;
                }
                const double adjustedStation =
                    std::max(openingLeft + kStudWidthMm / 2.0,
                             std::min(station, openingRight - kStudWidthMm / 2.0));
                const bool duplicatesJack =
                    std::any_of(jackStations.begin(), jackStations.end(),
                                [&](double existingStation) {
                                    return std::abs(existingStation - adjustedStation) < 0.001;
                                });
                if (duplicatesJack) { continue; }
                jackStations.push_back(adjustedStation);

                const double upperJackTop =
                    Interpolate(wallProfile.topStartMm, wallProfile.topEndMm,
                                adjustedStation, wallLength) -
                    static_cast<double>(kTopPlateCount) *
                        SlopedMemberVerticalHeight(kPlateHeightMm, topSlope);
                if (upperJackTop > upperJackBottom)
                {
                    const double height = upperJackTop - upperJackBottom;
                    addSegmentedJack(
                        adjustedStation, upperJackBottom, upperJackTop,
                        0.0, topSlope, i);
                }
                addLowerJack(adjustedStation);
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
                                const bool verticalOverlap =
                                    candidate.zStartMm <
                                        accepted.zStartMm + accepted.zHeightMm - 0.001 &&
                                    candidate.zStartMm + candidate.zHeightMm >
                                        accepted.zStartMm + 0.001;
                                return candidateLeft < acceptedRight - 0.001 &&
                                       candidateRight > acceptedLeft + 0.001 &&
                                       verticalOverlap;
                            });
            if (gSettings.resolveStudOverlaps && overlapsAccepted) { continue; }

            acceptedVerticalMembers.push_back(candidate);
            addVerticalMember(nextMemberName(MemberPrefix(candidate.type)), candidate.type,
                              candidate.stationMm, candidate.centrelineBottomMm,
                              candidate.centrelineTopMm, candidate.bottomSlope,
                              candidate.topSlope);
            ++result.studCount;
        }

        struct VerticalEdge
        {
            double left = 0.0;
            double right = 0.0;
            double bottom = 0.0;
            double top = 0.0;
            bool containsCornerStud = false;
        };

        std::vector<VerticalEdge> verticalEdges;
        for (const FrameMember& member : result.members)
        {
            if (member.type.find("STUD") == std::string::npos) { continue; }
            verticalEdges.push_back({ member.stationMm - member.widthMm / 2.0,
                                      member.stationMm + member.widthMm / 2.0,
                                      member.zStartMm, member.zEndMm,
                                      member.type == "CORNER_STUD" });
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
                mergedEdges.back().bottom = std::min(mergedEdges.back().bottom, edge.bottom);
                mergedEdges.back().top = std::max(mergedEdges.back().top, edge.top);
                mergedEdges.back().containsCornerStud =
                    mergedEdges.back().containsCornerStud || edge.containsCornerStud;
            }
        }

        auto supportsNogging = [&](const VerticalEdge& pack, double bottom, double top) {
            return std::any_of(verticalEdges.begin(), verticalEdges.end(),
                               [&](const VerticalEdge& edge) {
                                   const bool belongsToPack =
                                       edge.left < pack.right + 0.001 &&
                                       edge.right > pack.left - 0.001;
                                   return belongsToPack &&
                                          edge.bottom <= bottom + 0.001 &&
                                          edge.top >= top - 0.001;
                               });
        };

        auto tallestStudInPack = [&](const VerticalEdge& pack) {
            VerticalEdge tallest;
            double tallestHeight = 0.0;
            for (const VerticalEdge& edge : verticalEdges)
            {
                const bool belongsToPack =
                    edge.left < pack.right + 0.001 &&
                    edge.right > pack.left - 0.001;
                const double height = edge.top - edge.bottom;
                if (belongsToPack && height > tallestHeight)
                {
                    tallest = edge;
                    tallestHeight = height;
                }
            }
            return tallest;
        };

        auto supportingFace = [&](const VerticalEdge& pack, double bottom, double top,
                                  bool rightFace, double& face) {
            bool found = false;
            for (const VerticalEdge& edge : verticalEdges)
            {
                const bool belongsToPack =
                    edge.left < pack.right + 0.001 &&
                    edge.right > pack.left - 0.001;
                if (!belongsToPack ||
                    edge.bottom > bottom + 0.001 ||
                    edge.top < top - 0.001)
                {
                    continue;
                }

                const double candidate = rightFace ? edge.right : edge.left;
                if (!found || (rightFace ? candidate > face : candidate < face))
                {
                    face = candidate;
                    found = true;
                }
            }
            return found;
        };

        const double highestStudHeight =
            std::max(wallProfile.topStartMm - wallProfile.bottomStartMm,
                     wallProfile.topEndMm - wallProfile.bottomEndMm) -
            static_cast<double>(kBottomPlateCount) *
                SlopedMemberVerticalHeight(kPlateHeightMm, bottomSlope) -
            static_cast<double>(kTopPlateCount) *
                SlopedMemberVerticalHeight(kPlateHeightMm, topSlope);
        const size_t noggingRowCount =
            gSettings.generateNoggings && highestStudHeight > 0.0
                ? static_cast<size_t>(
                      std::max(0.0, std::ceil(highestStudHeight / kNoggingCentresMm) - 1.0))
                : 0;
        for (size_t row = 1; row <= noggingRowCount; ++row)
        {
            size_t noggingIndex = 0;
            for (size_t i = 1; i < mergedEdges.size(); ++i)
            {
                const VerticalEdge& leftPack = mergedEdges[i - 1];
                const VerticalEdge& rightPack = mergedEdges[i];
                const double packGapLength = rightPack.left - leftPack.right;
                const bool cornerClusterGap =
                    leftPack.containsCornerStud || rightPack.containsCornerStud;
                if (packGapLength <= 0.001 ||
                    (!cornerClusterGap && packGapLength <= kStudWidthMm + 0.001))
                {
                    continue;
                }

                const VerticalEdge leftTallest = tallestStudInPack(leftPack);
                const VerticalEdge rightTallest = tallestStudInPack(rightPack);
                const VerticalEdge& governingStud =
                    leftTallest.top - leftTallest.bottom >= rightTallest.top - rightTallest.bottom
                        ? leftTallest
                        : rightTallest;
                const double governingHeight = governingStud.top - governingStud.bottom;
                const size_t pairNoggingRowCount =
                    governingHeight > 0.0
                        ? static_cast<size_t>(
                              std::max(0.0, std::ceil(governingHeight / kNoggingCentresMm) - 1.0))
                        : 0;
                if (row > pairNoggingRowCount) { continue; }

                const double noggingCentreZ =
                    governingStud.bottom +
                    governingHeight * static_cast<double>(row) /
                        static_cast<double>(pairNoggingRowCount + 1);
                const double noggingZ =
                    noggingIndex % 2 == 0 ? noggingCentreZ - gSettings.noggingStaggerMm
                                         : noggingCentreZ;
                const double noggingTop = noggingZ + kNoggingHeightMm;
                if (!supportsNogging(leftPack, noggingZ, noggingTop) ||
                    !supportsNogging(rightPack, noggingZ, noggingTop))
                {
                    continue;
                }

                double gapStart = 0.0;
                double gapEnd = 0.0;
                if (!supportingFace(leftPack, noggingZ, noggingTop, true, gapStart) ||
                    !supportingFace(rightPack, noggingZ, noggingTop, false, gapEnd))
                {
                    continue;
                }
                const double gapLength = gapEnd - gapStart;
                if (gapLength <= 0.001 ||
                    (!cornerClusterGap && gapLength <= kStudWidthMm + 0.001))
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
        settings.ledgerTriggerHeightMm = std::max(0.0, settings.ledgerTriggerHeightMm);
        settings.noggingCentresMm = std::max(settings.noggingHeightMm, settings.noggingCentresMm);
        settings.noggingStaggerMm = std::max(0.0, settings.noggingStaggerMm);
        settings.bottomPlateCount = std::max<Sint32>(0, settings.bottomPlateCount);
        settings.topPlateCount = std::max<Sint32>(0, settings.topPlateCount);
        settings.jambStudCount = std::max<Sint32>(0, settings.jambStudCount);
        settings.trimmerStudCount = std::max<Sint32>(1, settings.trimmerStudCount);
        if (settings.bottomPlatePrefix.IsEmpty()) { settings.bottomPlatePrefix = "BP"; }
        if (settings.topPlatePrefix.IsEmpty()) { settings.topPlatePrefix = "TP"; }
        if (settings.studPrefix.IsEmpty()) { settings.studPrefix = "S"; }
        if (settings.endStudPrefix.IsEmpty()) { settings.endStudPrefix = "ES"; }
        if (settings.cornerStudPrefix.IsEmpty()) { settings.cornerStudPrefix = "CS"; }
        if (settings.jambStudPrefix.IsEmpty()) { settings.jambStudPrefix = "JAM"; }
        if (settings.trimmerStudPrefix.IsEmpty()) { settings.trimmerStudPrefix = "TR"; }
        if (settings.jackStudPrefix.IsEmpty()) { settings.jackStudPrefix = "JS"; }
        if (settings.noggingPrefix.IsEmpty()) { settings.noggingPrefix = "NOG"; }
        if (settings.windowSillPrefix.IsEmpty()) { settings.windowSillPrefix = "WS"; }
        if (settings.lintelPrefix.IsEmpty()) { settings.lintelPrefix = "LIN"; }
        if (settings.doorHeadPrefix.IsEmpty()) { settings.doorHeadPrefix = "DH"; }
        if (settings.ledgerPrefix.IsEmpty()) { settings.ledgerPrefix = "LED"; }
    }

    std::vector<WallOpening> FindSelectedWallOpenings(const std::vector<MCObjectHandle>& walls)
    {
        std::vector<WallOpening> openings;
        for (MCObjectHandle wall : walls)
        {
            VWFC::VWObjects::VWWallObj wallObj(wall);
            if (wallObj.IsRound()) { continue; }

            const auto start = wallObj.GetStartPoint();
            const auto end = wallObj.GetEndPoint();
            const double wallLength = Distance(start, end);
            WallVerticalProfile wallProfile;
            if (wallLength <= 0.0 || !GetLinearWallProfile(wallObj, wallProfile)) { continue; }

            const FramingComponent component = FindFramingComponent(wall, wallObj);
            if (!component.found) { continue; }
            wallProfile.bottomStartMm += component.bottomOffsetMm;
            wallProfile.bottomEndMm += component.bottomOffsetMm;

            const std::vector<WallOpening> wallOpenings =
                FindWallOpenings(wallObj, wallProfile.bottomStartMm,
                                 wallProfile.bottomEndMm, wallLength);
            openings.insert(openings.end(), wallOpenings.begin(), wallOpenings.end());
        }
        return openings;
    }

    enum class EOpeningListColumn
    {
        UserID = 0,
        Type,
        OpeningSize,
        LintelID,
        LintelCount,
        LintelWidth,
        LintelHeight,
        LowerLedger,
        UpperLedger,
        JackStudOverrun,
        SillSupportJacks
    };

    class CFramingSettingsDialog : public VWDialog
    {
    public:
        CFramingSettingsDialog(const std::vector<MCObjectHandle>& walls)
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
              fGenerateUpperLedgers(202), fGenerateLedgers(195), fLedgerTriggerHeightLabel(198),
              fLedgerTriggerHeightEdit(199), fContinueJackStudsToLintelUnderside(200),
              fJambStudCountLabel(142), fJambStudCountEdit(143),
              fTrimmerStudCountLabel(144), fTrimmerStudCountEdit(145),
              fGenerateNoggings(150), fNoggingHeightLabel(151), fNoggingHeightEdit(152),
              fNoggingCentresLabel(153), fNoggingCentresEdit(154),
              fNoggingStaggerLabel(155), fNoggingStaggerEdit(156),
              fGenerateCornerStuds(160), fResolveStudOverlaps(161),
              fBottomPlatePrefixLabel(162), fBottomPlatePrefixEdit(163),
              fTopPlatePrefixLabel(164), fTopPlatePrefixEdit(165),
              fStudPrefixLabel(166), fStudPrefixEdit(167),
              fEndStudPrefixLabel(185), fEndStudPrefixEdit(186),
              fCornerStudPrefixLabel(187), fCornerStudPrefixEdit(188),
              fJambStudPrefixLabel(189), fJambStudPrefixEdit(190),
              fTrimmerStudPrefixLabel(191), fTrimmerStudPrefixEdit(192),
              fJackStudPrefixLabel(193), fJackStudPrefixEdit(194),
              fNoggingPrefixLabel(168), fNoggingPrefixEdit(169),
              fWindowSillPrefixLabel(170), fWindowSillPrefixEdit(171),
              fLintelPrefixLabel(172), fLintelPrefixEdit(173),
              fDoorHeadPrefixLabel(174), fDoorHeadPrefixEdit(175),
              fLedgerPrefixLabel(196), fLedgerPrefixEdit(197),
              fOpeningList(201),
              fProfileMemberHeader(176), fProfileWidthHeader(177), fProfileHeightHeader(178),
              fStudDepthEdit(179), fPlateWidthEdit(180), fNoggingWidthEdit(181),
              fSillLabel(182), fSillWidthEdit(183), fSillHeightEdit(184)
        {
            fSettings = gSettings;
            fOpeningRows = FindSelectedWallOpenings(walls);
            std::map<std::string, size_t> labelCounts;
            for (WallOpening& opening : fOpeningRows)
            {
                const std::string baseLabel =
                    opening.userID.empty()
                        ? (opening.type == "WINDOW" ? "Unnamed window" : "Unnamed door")
                        : opening.userID;
                const size_t count = ++labelCounts[baseLabel];
                opening.userID =
                    count == 1 ? baseLabel : baseLabel + " (" + std::to_string(count) + ")";
            }
        }

        bool Run()
        {
            if (this->RunDialogLayout("iQsWallFramerSettings") != kDialogButton_Ok)
            {
                return false;
            }
        SanitizeSettings(fSettings);
        gSettings = fSettings;
        for (const WallOpening& opening : fOpeningRows)
        {
            gOpeningLintelOverrides[opening.key] =
                { opening.lintelWidthMm, opening.lintelHeightMm,
                  opening.lintelCount, opening.lintelID,
                  opening.lowerLedger, opening.upperLedger,
                  opening.continueJackStudsToLintelUnderside,
                  opening.sillSupportJacks };
            MCObjectHandle record = EnsureOpeningRecord(opening.handle);
            SetRecordText(record, "lintel_id", opening.lintelID.c_str());
            SetRecordText(record, "lintel_count", TXString::ToStringInt(opening.lintelCount));
            SetRecordText(record, "lintel_width_mm", Num(opening.lintelWidthMm).c_str());
            SetRecordText(record, "lintel_height_mm", Num(opening.lintelHeightMm).c_str());
            SetRecordText(record, "lower_ledger", Bool(opening.lowerLedger).c_str());
            SetRecordText(record, "upper_ledger", Bool(opening.upperLedger).c_str());
            SetRecordText(record, "continue_jack_studs_to_lintel_underside",
                          Bool(opening.continueJackStudsToLintelUnderside).c_str());
            SetRecordText(record, "sill_support_jacks",
                          Bool(opening.sillSupportJacks).c_str());
            SetRecordText(record, "last_framed_unix",
                          TXString::ToStringInt(static_cast<Sint32>(Now())));
        }
        return true;
        }

    protected:
        virtual bool CreateDialogLayout() override
        {
            if (!this->CreateDialog("iQs Wall Framer Settings", "Generate", "Cancel",
                                    false, true, false))
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
                !fGenerateUpperLedgers.CreateControl(this, "Default: add ledgers above deep lintels") ||
                !fGenerateLedgers.CreateControl(this, "Default: add ledgers beneath deep lintels") ||
                !fLedgerTriggerHeightLabel.CreateControl(this, "Ledger required above lintel height:", 34) ||
                !fLedgerTriggerHeightEdit.CreateControl(this, fSettings.ledgerTriggerHeightMm, 14, VWEditRealCtrl::kEditControlDimension) ||
                !fContinueJackStudsToLintelUnderside.CreateControl(this, "Default: continue jack studs to underside of lintel") ||
                !fOpeningList.CreateControl(this, 172, 10) ||
                !fJambStudCountLabel.CreateControl(this, "Jamb studs per opening side:", 28) ||
                !fJambStudCountEdit.CreateControl(this, fSettings.jambStudCount, 14) ||
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
                !fEndStudPrefixLabel.CreateControl(this, "End stud prefix:", 22) ||
                !fEndStudPrefixEdit.CreateControl(this, fSettings.endStudPrefix, 12) ||
                !fCornerStudPrefixLabel.CreateControl(this, "Corner stud prefix:", 22) ||
                !fCornerStudPrefixEdit.CreateControl(this, fSettings.cornerStudPrefix, 12) ||
                !fJambStudPrefixLabel.CreateControl(this, "Jamb stud prefix:", 22) ||
                !fJambStudPrefixEdit.CreateControl(this, fSettings.jambStudPrefix, 12) ||
                !fTrimmerStudPrefixLabel.CreateControl(this, "Trimmer stud prefix:", 22) ||
                !fTrimmerStudPrefixEdit.CreateControl(this, fSettings.trimmerStudPrefix, 12) ||
                !fJackStudPrefixLabel.CreateControl(this, "Jack stud prefix:", 22) ||
                !fJackStudPrefixEdit.CreateControl(this, fSettings.jackStudPrefix, 12) ||
                !fNoggingPrefixLabel.CreateControl(this, "Nogging prefix:", 22) ||
                !fNoggingPrefixEdit.CreateControl(this, fSettings.noggingPrefix, 12) ||
                !fWindowSillPrefixLabel.CreateControl(this, "Window sill prefix:", 22) ||
                !fWindowSillPrefixEdit.CreateControl(this, fSettings.windowSillPrefix, 12) ||
                !fLintelPrefixLabel.CreateControl(this, "Lintel prefix:", 22) ||
                !fLintelPrefixEdit.CreateControl(this, fSettings.lintelPrefix, 12) ||
                !fDoorHeadPrefixLabel.CreateControl(this, "Door head prefix:", 22) ||
                !fDoorHeadPrefixEdit.CreateControl(this, fSettings.doorHeadPrefix, 12) ||
                !fLedgerPrefixLabel.CreateControl(this, "Ledger prefix:", 22) ||
                !fLedgerPrefixEdit.CreateControl(this, fSettings.ledgerPrefix, 12))
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
            this->AddBelowControl(&fDetectWindows, &fGenerateUpperLedgers);
            this->AddBelowControl(&fGenerateUpperLedgers, &fGenerateLedgers);
            this->AddBelowControl(&fGenerateLedgers, &fLedgerTriggerHeightLabel);
            this->AddRightControl(&fLedgerTriggerHeightLabel, &fLedgerTriggerHeightEdit);
            this->AddBelowControl(&fLedgerTriggerHeightLabel, &fContinueJackStudsToLintelUnderside);
            this->AddBelowControl(&fContinueJackStudsToLintelUnderside, &fJambStudCountLabel);
            this->AddRightControl(&fJambStudCountLabel, &fJambStudCountEdit);
            AddLabelledControl(fTrimmerStudCountLabel, fTrimmerStudCountEdit, &fJambStudCountLabel);
            this->AddBelowControl(&fTrimmerStudCountLabel, &fOpeningList);

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
            AddLabelledControl(fEndStudPrefixLabel, fEndStudPrefixEdit, &fStudPrefixLabel);
            AddLabelledControl(fCornerStudPrefixLabel, fCornerStudPrefixEdit, &fEndStudPrefixLabel);
            AddLabelledControl(fJambStudPrefixLabel, fJambStudPrefixEdit, &fCornerStudPrefixLabel);
            AddLabelledControl(fTrimmerStudPrefixLabel, fTrimmerStudPrefixEdit, &fJambStudPrefixLabel);
            AddLabelledControl(fJackStudPrefixLabel, fJackStudPrefixEdit, &fTrimmerStudPrefixLabel);
            AddLabelledControl(fNoggingPrefixLabel, fNoggingPrefixEdit, &fJackStudPrefixLabel);
            AddLabelledControl(fWindowSillPrefixLabel, fWindowSillPrefixEdit, &fNoggingPrefixLabel);
            AddLabelledControl(fLintelPrefixLabel, fLintelPrefixEdit, &fWindowSillPrefixLabel);
            AddLabelledControl(fDoorHeadPrefixLabel, fDoorHeadPrefixEdit, &fLintelPrefixLabel);
            AddLabelledControl(fLedgerPrefixLabel, fLedgerPrefixEdit, &fDoorHeadPrefixLabel);
            return true;
        }

        virtual void OnInitializeContent() override
        {
            VWDialog::OnInitializeContent();
            fOpeningList.EnableSorting(false);
            fOpeningList.EnableDirectEdit(true);
            fOpeningList.EnableColumnLines(true);
            fOpeningList.AddColumn("Window / door ID", 150);
            fOpeningList.AddColumn("Type", 80);
            fOpeningList.AddColumn("Opening size", 110);
            fOpeningList.AddColumn("Lintel ID", 80);
            fOpeningList.AddColumn("Count", 55);
            fOpeningList.AddColumn("Lintel width", 90);
            fOpeningList.AddColumn("Lintel height", 90);
            fOpeningList.AddColumn("Lower ledger", 85);
            fOpeningList.AddColumn("Upper ledger", 85);
            fOpeningList.AddColumn("Jack overrun", 85);
            fOpeningList.AddColumn("Sill jacks", 85);
            for (size_t row = 0; row < fOpeningRows.size(); ++row)
            {
                fOpeningList.AddRow("");
                SetOpeningRow(row);
            }
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
            this->AddDDX_CheckButton(202, &fSettings.generateUpperLedgers);
            this->AddDDX_CheckButton(195, &fSettings.generateLedgers);
            this->AddDDX_EditReal(199, &fSettings.ledgerTriggerHeightMm, VWEditRealCtrl::kEditControlDimension);
            this->AddDDX_CheckButton(200, &fSettings.continueJackStudsToLintelUnderside);
            this->AddDDX_EditInteger(143, &fSettings.jambStudCount);
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
            this->AddDDX_EditText(186, &fSettings.endStudPrefix);
            this->AddDDX_EditText(188, &fSettings.cornerStudPrefix);
            this->AddDDX_EditText(190, &fSettings.jambStudPrefix);
            this->AddDDX_EditText(192, &fSettings.trimmerStudPrefix);
            this->AddDDX_EditText(194, &fSettings.jackStudPrefix);
            this->AddDDX_EditText(169, &fSettings.noggingPrefix);
            this->AddDDX_EditText(171, &fSettings.windowSillPrefix);
            this->AddDDX_EditText(173, &fSettings.lintelPrefix);
            this->AddDDX_EditText(175, &fSettings.doorHeadPrefix);
            this->AddDDX_EditText(197, &fSettings.ledgerPrefix);
        }

    private:
        void SetOpeningRow(size_t row)
        {
            if (row >= fOpeningRows.size()) { return; }
            const WallOpening& opening = fOpeningRows[row];
            fOpeningList.GetItem(row, static_cast<size_t>(EOpeningListColumn::UserID))
                .SetItemText(opening.userID.c_str());
            fOpeningList.GetItem(row, static_cast<size_t>(EOpeningListColumn::Type))
                .SetItemText(opening.type == "WINDOW" ? "Window" : "Door");
            fOpeningList.GetItem(row, static_cast<size_t>(EOpeningListColumn::OpeningSize))
                .SetItemText((WholeMm(opening.widthMm) + " x " +
                              WholeMm(opening.topMm - opening.bottomMm)).c_str());
            VWListBrowserItem lintelIDItem =
                fOpeningList.GetItem(row, static_cast<size_t>(EOpeningListColumn::LintelID));
            lintelIDItem.SetItemText(opening.lintelID.c_str());
            lintelIDItem.SetItemInteractionType(kListBrowserItemInteractionEditText);
            VWListBrowserItem countItem =
                fOpeningList.GetItem(row, static_cast<size_t>(EOpeningListColumn::LintelCount));
            countItem.SetItemText(std::to_string(opening.lintelCount).c_str());
            countItem.SetItemInteractionType(kListBrowserItemInteractionEditText);
            VWListBrowserItem widthItem =
                fOpeningList.GetItem(row, static_cast<size_t>(EOpeningListColumn::LintelWidth));
            widthItem.SetItemText(WholeMm(opening.lintelWidthMm).c_str());
            widthItem.SetItemInteractionType(kListBrowserItemInteractionEditText);
            VWListBrowserItem heightItem =
                fOpeningList.GetItem(row, static_cast<size_t>(EOpeningListColumn::LintelHeight));
            heightItem.SetItemText(WholeMm(opening.lintelHeightMm).c_str());
            heightItem.SetItemInteractionType(kListBrowserItemInteractionEditText);
            VWListBrowserItem lowerLedgerItem =
                fOpeningList.GetItem(row, static_cast<size_t>(EOpeningListColumn::LowerLedger));
            lowerLedgerItem.SetItemInteractionType(kListBrowserItemInteractionEditCheckState);
            lowerLedgerItem.SetItemCheckState(
                opening.lowerLedger ? CGSMultiStateValueChange::eStateValueOn
                                    : CGSMultiStateValueChange::eStateValueOff);
            VWListBrowserItem upperLedgerItem =
                fOpeningList.GetItem(row, static_cast<size_t>(EOpeningListColumn::UpperLedger));
            upperLedgerItem.SetItemInteractionType(kListBrowserItemInteractionEditCheckState);
            upperLedgerItem.SetItemCheckState(
                opening.upperLedger ? CGSMultiStateValueChange::eStateValueOn
                                    : CGSMultiStateValueChange::eStateValueOff);
            VWListBrowserItem jackStudOverrunItem =
                fOpeningList.GetItem(row, static_cast<size_t>(EOpeningListColumn::JackStudOverrun));
            jackStudOverrunItem.SetItemInteractionType(kListBrowserItemInteractionEditCheckState);
            jackStudOverrunItem.SetItemCheckState(
                opening.continueJackStudsToLintelUnderside
                    ? CGSMultiStateValueChange::eStateValueOn
                    : CGSMultiStateValueChange::eStateValueOff);
            VWListBrowserItem sillSupportJacksItem =
                fOpeningList.GetItem(row, static_cast<size_t>(EOpeningListColumn::SillSupportJacks));
            sillSupportJacksItem.SetItemInteractionType(kListBrowserItemInteractionEditCheckState);
            sillSupportJacksItem.SetItemCheckState(
                opening.sillSupportJacks ? CGSMultiStateValueChange::eStateValueOn
                                         : CGSMultiStateValueChange::eStateValueOff);
        }

        void OnOpeningListDirectEdit(TControlID, VWListBrowserEventArgs& eventArgs)
        {
            size_t row = 0;
            size_t column = 0;
            const EListBrowserDirectEditType type = eventArgs.GetType(row, column);
            if (row >= fOpeningRows.size())
            {
                return;
            }

            WallOpening& opening = fOpeningRows[row];
            const bool ledgerColumn =
                column == static_cast<size_t>(EOpeningListColumn::LowerLedger) ||
                column == static_cast<size_t>(EOpeningListColumn::UpperLedger);
            const bool checkboxColumn =
                ledgerColumn ||
                column == static_cast<size_t>(EOpeningListColumn::JackStudOverrun) ||
                column == static_cast<size_t>(EOpeningListColumn::SillSupportJacks);
            if (checkboxColumn &&
                (type == EListBrowserDirectEditType::QueryItemListRetrieval ||
                 type == EListBrowserDirectEditType::QueryItemValue))
            {
                eventArgs.GetCellCheckbox().fStateValue =
                    (column == static_cast<size_t>(EOpeningListColumn::LowerLedger)
                         ? opening.lowerLedger
                         : column == static_cast<size_t>(EOpeningListColumn::UpperLedger)
                               ? opening.upperLedger
                               : column == static_cast<size_t>(EOpeningListColumn::JackStudOverrun)
                                     ? opening.continueJackStudsToLintelUnderside
                                     : opening.sillSupportJacks)
                        ? CGSMultiStateValueChange::eStateValueOn
                        : CGSMultiStateValueChange::eStateValueOff;
                return;
            }

            if (type != EListBrowserDirectEditType::ItemEditCompletionData)
            {
                return;
            }

            if (column == static_cast<size_t>(EOpeningListColumn::LowerLedger) ||
                column == static_cast<size_t>(EOpeningListColumn::UpperLedger) ||
                column == static_cast<size_t>(EOpeningListColumn::JackStudOverrun) ||
                column == static_cast<size_t>(EOpeningListColumn::SillSupportJacks))
            {
                const bool enabled =
                    eventArgs.GetCellCheckbox().fStateValue ==
                    CGSMultiStateValueChange::eStateValueOn;
                if (column == static_cast<size_t>(EOpeningListColumn::LowerLedger))
                {
                    opening.lowerLedger = enabled;
                }
                else
                {
                    if (column == static_cast<size_t>(EOpeningListColumn::UpperLedger))
                    {
                        opening.upperLedger = enabled;
                    }
                    else
                    {
                        if (column == static_cast<size_t>(EOpeningListColumn::JackStudOverrun))
                        {
                            opening.continueJackStudsToLintelUnderside = enabled;
                        }
                        else
                        {
                            opening.sillSupportJacks = enabled;
                        }
                    }
                }
                SetOpeningRow(row);
                eventArgs.SetValidEditCompletionData();
                return;
            }

            if (column != static_cast<size_t>(EOpeningListColumn::LintelID) &&
                column != static_cast<size_t>(EOpeningListColumn::LintelCount) &&
                column != static_cast<size_t>(EOpeningListColumn::LintelWidth) &&
                column != static_cast<size_t>(EOpeningListColumn::LintelHeight))
            {
                return;
            }

            const TXString& value = eventArgs.GetCellString().fNewStringValue;
            if (column == static_cast<size_t>(EOpeningListColumn::LintelID))
            {
                opening.lintelID = value.GetCharPtr();
                SetOpeningRow(row);
                eventArgs.SetValidEditCompletionData();
                return;
            }

            if (column == static_cast<size_t>(EOpeningListColumn::LintelCount))
            {
                Sint32 parsed = 0;
                try
                {
                    parsed = static_cast<Sint32>(std::stoi(value.GetCharPtr()));
                }
                catch (...)
                {
                    eventArgs.SetInvalidEditCompletionData();
                    return;
                }
                if (parsed <= 0)
                {
                    eventArgs.SetInvalidEditCompletionData();
                    return;
                }
                opening.lintelCount = parsed;
                SetOpeningRow(row);
                eventArgs.SetValidEditCompletionData();
                return;
            }

            double parsed = 0.0;
            try
            {
                parsed = std::stod(value.GetCharPtr());
            }
            catch (...)
            {
                eventArgs.SetInvalidEditCompletionData();
                return;
            }
            if (parsed <= 0.0)
            {
                eventArgs.SetInvalidEditCompletionData();
                return;
            }

            if (column == static_cast<size_t>(EOpeningListColumn::LintelWidth))
            {
                opening.lintelWidthMm = parsed;
            }
            else
            {
                opening.lintelHeightMm = parsed;
            }
            SetOpeningRow(row);
            eventArgs.SetValidEditCompletionData();
        }

        DEFINE_EVENT_DISPATH_MAP;

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
        std::vector<WallOpening> fOpeningRows;
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
        VWCheckButtonCtrl fGenerateUpperLedgers;
        VWCheckButtonCtrl fGenerateLedgers;
        VWStaticTextCtrl fLedgerTriggerHeightLabel;
        VWEditRealCtrl fLedgerTriggerHeightEdit;
        VWCheckButtonCtrl fContinueJackStudsToLintelUnderside;
        VWStaticTextCtrl fJambStudCountLabel;
        VWEditIntegerCtrl fJambStudCountEdit;
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
        VWStaticTextCtrl fEndStudPrefixLabel;
        VWEditTextCtrl fEndStudPrefixEdit;
        VWStaticTextCtrl fCornerStudPrefixLabel;
        VWEditTextCtrl fCornerStudPrefixEdit;
        VWStaticTextCtrl fJambStudPrefixLabel;
        VWEditTextCtrl fJambStudPrefixEdit;
        VWStaticTextCtrl fTrimmerStudPrefixLabel;
        VWEditTextCtrl fTrimmerStudPrefixEdit;
        VWStaticTextCtrl fJackStudPrefixLabel;
        VWEditTextCtrl fJackStudPrefixEdit;
        VWStaticTextCtrl fNoggingPrefixLabel;
        VWEditTextCtrl fNoggingPrefixEdit;
        VWStaticTextCtrl fWindowSillPrefixLabel;
        VWEditTextCtrl fWindowSillPrefixEdit;
        VWStaticTextCtrl fLintelPrefixLabel;
        VWEditTextCtrl fLintelPrefixEdit;
        VWStaticTextCtrl fDoorHeadPrefixLabel;
        VWEditTextCtrl fDoorHeadPrefixEdit;
        VWStaticTextCtrl fLedgerPrefixLabel;
        VWEditTextCtrl fLedgerPrefixEdit;
        VWListBrowserCtrl fOpeningList;
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

    EVENT_DISPATCH_MAP_BEGIN(CFramingSettingsDialog);
    ADD_LB_DIRECT_EDIT(201, OnOpeningListDirectEdit);
    EVENT_DISPATCH_MAP_END;
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

        CFramingSettingsDialog settingsDialog(walls);
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
            std::map<std::string, size_t> memberNameCounters;
            for (MCObjectHandle wall : walls)
            {
                VWFC::VWObjects::VWWallObj wallObj(wall);
                if (!IsSupportedWallForV1(wallObj))
                {
                    if (reportCleanup) { ++unsupportedWallCount; }
                    continue;
                }
                const std::vector<FramingComponent> components =
                    FindFramingComponents(wall, wallObj);
                if (components.empty())
                {
                    if (reportCleanup) { ++missingComponentCount; }
                    continue;
                }

                const TXString hostUUID = EnsureHostUUID(wall);
                const std::vector<MCObjectHandle> linkedFrames = FindLinkedFrames(hostUUID);
                if (reportCleanup && linkedFrames.size() > components.size())
                {
                    ++duplicateHostCount;
                    duplicateFrameCount += linkedFrames.size();
                }
                for (MCObjectHandle linkedFrame : linkedFrames)
                {
                    gSDK->DeleteObject(linkedFrame, true);
                    if (reportCleanup) { ++refreshedFrameCount; }
                }

                const bool includeComponentIndex = components.size() > 1;
                for (const FramingComponent& component : components)
                {
                    GeneratedFrame frame = GenerateSimpleFrame(
                        wall, component, FindPlateExtents(wall, contextWalls, component),
                        includeComponentIndex, memberNameCounters);
                    if (frame.group) { frames.push_back(frame); }
                }
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
