/** @file CPlayerProgress.h
 * @brief Original experience and health tables, with local progress state.
 */
#ifndef GUN_BROS_RE_CPLAYERPROGRESS_H
#define GUN_BROS_RE_CPLAYERPROGRESS_H
#include "engine/resources/CArrayInputStream.h"
#include <vector>

class CPlayerProgress {
public:
    class Template {
    public:
        bool Init(CArrayInputStream &stream);
        unsigned GetMaximumLevel() const;
        std::uint64_t GetExperienceForLevel(unsigned level) const;
        std::vector<std::uint32_t> experience;
        std::vector<std::int16_t> health;
        // Preserve unnamed tail fields until their consumers are established.
        std::int32_t value20Raw = 0;
        float value20 = 0;
        std::int32_t value24 = 0;
    };

    void Bind(const Template &data);
    void SetExperience(std::uint64_t experience);
    /** Runtime CPlayer uses >= at a threshold (:101185); disk helper uses >. */
    bool AddExperience(std::uint32_t amount);
    unsigned GetLevel() const { return m_level; }
    std::uint64_t GetExperience() const { return m_experience; }
    std::uint64_t GetExperienceInLevel() const;
    std::uint32_t GetExperienceDelta() const;
    float GetHealth() const;
    bool IsMaximumLevel() const;

private:
    const Template *m_template = nullptr;
    std::uint64_t m_experience = 0;
    unsigned m_level = 1;
};
#endif
