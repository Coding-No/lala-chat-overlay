#pragma once
#include <string>

enum class UserRole {
    Regular,
    Verified,
    Member,
    Moderator,
    Owner
};

inline const char* UserRoleToString(UserRole role) {
    switch (role) {
        case UserRole::Owner: return "OWNER";
        case UserRole::Moderator: return "MOD";
        case UserRole::Member: return "MEMBER";
        case UserRole::Verified: return "VERIFIED";
        case UserRole::Regular: return "USER";
        default: return "USER";
    }
}

inline UserRole StringToUserRole(const std::string& str) {
    if (str == "OWNER" || str == "Owner") return UserRole::Owner;
    if (str == "MOD" || str == "Moderator") return UserRole::Moderator;
    if (str == "MEMBER" || str == "Member") return UserRole::Member;
    if (str == "VERIFIED" || str == "Verified") return UserRole::Verified;
    return UserRole::Regular;
}
