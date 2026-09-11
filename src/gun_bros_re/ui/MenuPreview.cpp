#include "gun_bros_re/ui/MenuInternal.h"

namespace MenuDetail {

    bool GameMenu::DrawEquippedPlayer(CResTOCManager &toc, PackTables &tables, const CProfileManager &profile,
        const std::vector<WeaponEntry> &weapons, const std::vector<ArmorEntry> &armors, unsigned slot,
        const GameObjectTypeRef *previewItem , const MovieRegion *storePanel , float spin ) {
        unsigned gunSlot = profile.activeWeaponSlot;
        if (slot < 2) { gunSlot = slot; }
        // Preview substitutes only the model configuration. Ownership, currency
        // and the saved loadout remain owned by the explicit purchase action.
        CPlayerConfiguration configuration = profile.configuration;
        if (previewItem != nullptr) {
            if (previewItem->type == 6) { configuration.guns[gunSlot] = previewItem->object; }
            if (previewItem->type == 2 && slot >= 2 && slot <= 4) { configuration.armor[kArmorSlots[slot]] = previewItem->object; }
        }
        // Switching weapon slot is the original's swap, not just a rebuild.
        bool changed = equippedPreview == nullptr;
        if (equippedPreview != nullptr && equippedPreview->brotherIndex != profile.playerBrother) { changed = true; }
        for (unsigned index = 0; index < 2; ++index) {
            if (!SameObject(previewConfiguration.guns[index], configuration.guns[index])) { changed = true; }
        }
        for (unsigned index = 0; index < kArmorSlotCount; ++index) {
            if (!SameObject(previewConfiguration.armor[index], configuration.armor[index])) { changed = true; }
        }
        if (changed) {
            PlayerTemplateData playerTemplate;
            if (!FindPlayerTemplate(toc, tables, playerTemplate)) { return false; }
            auto candidate = std::make_unique<PlayerModel>();
            candidate->brotherIndex = profile.playerBrother;
            if (!BuildPlayerBody(tables, playerTemplate.moveSet, *candidate)) { return false; }
            const WeaponEntry *gun = nullptr;
            for (const WeaponEntry &entry : weapons) {
                const auto &ref = configuration.guns[gunSlot];
                if (entry.packHash == ref.packHash && entry.ordinal == ref.localIndex) { gun = &entry; break; }
            }
            if (gun == nullptr || !EquipPlayerWeapon(tables, playerTemplate.script, gun->data, gun->owner, *candidate)) { return false; }
            const GameObjectRef &otherRef = configuration.guns[1 - gunSlot];
            if (!otherRef.IsNull()) {
                const WeaponEntry *otherGun = nullptr;
                for (const WeaponEntry &entry : weapons) {
                    if (entry.packHash == otherRef.packHash && entry.ordinal == otherRef.localIndex) { otherGun = &entry; break; }
                }
                if (otherGun == nullptr || !PreparePlayerUIWeapon(tables, otherGun->data, otherGun->owner, *candidate)) { return false; }
            }
            if (!candidate->weapon->brother.SpawnForUI()) { return false; }
            const auto &uiTorso = candidate->weapon->brother.GetTorso();
            std::printf("[player-ui] gun=%s state=%d weapon-torso=%d move=%d config=%d time=%d range=%d..%d override9=%d\n",
                gun->owner.c_str(), candidate->weapon->brother.GetStateId(), candidate->weapon->brother.TorsoUsesWeapon(),
                uiTorso.GetMoveIndex(), uiTorso.GetMeshConfigIndex(), uiTorso.GetAnimation().GetTimeMs(),
                uiTorso.GetAnimation().GetRangeStartMs(), uiTorso.GetAnimation().GetRangeStartMs() + uiTorso.GetAnimation().GetRangeDurationMs(),
                candidate->weapon->gun.GetOverrides()[9]);
            for (unsigned index = 0; index < kArmorSlotCount; ++index) {
                const auto &ref = configuration.armor[index];
                if (ref.IsNull()) { continue; }
                for (const ArmorEntry &entry : armors) {
                    if (entry.packHash == ref.packHash && entry.ordinal == ref.localIndex) {
                        if (!EquipPlayerArmor(tables, entry.data, imageProgram, *candidate)) { return false; }
                        break;
                    }
                }
            }
            if (!CreatePlayerBuffers(*candidate, imageProgram)) { return false; }
            equippedPreview = std::move(candidate);
            previewConfiguration = configuration;
            previewGunSlot = gunSlot;
            previewPrimarySlot = gunSlot;
            previewSwapPending = false;
            previewSlotChanged = false;
            previewTicks = clock;
            // CPlayer::OnSwapGun :101048 hands input event 5 to the player
            // script, which owns the swap animation.
        }
        if (previewGunSlot != gunSlot && !previewSwapPending && equippedPreview->uiOtherWeapon) {
            previewSwapPending = equippedPreview->weapon->brother.OnSwapGun();
        }
        if (storePanel == nullptr) { return false; }
        const std::uint64_t now = clock;
        unsigned previewDelta = 0;
        if (now >= previewTicks) { previewDelta = static_cast<unsigned>(std::min<std::uint64_t>(now - previewTicks, 100)); }
        AdvancePlayerPreview(previewDelta);
        PosePlayer(*equippedPreview);
        previewTicks = now;
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        float model[16];
        if (storePanel != nullptr) {
            // CMenuStore::Bind :180082 supplies region 2 as mesh bounds.
            // CMenuMeshPlayer::Draw :169591 calls DrawUI in the full page;
            // that region is not a viewport or a scissor rectangle.
            if (!BuildPlayerUIMatrix(*equippedPreview, storePanel->x + static_cast<int>(storePanel->width) / 2,
                storePanel->y, storePanel->height, spin, kMenuWidth, kMenuHeight, model)) { return false; }
            glViewport(0, 0, width, height);
            glDisable(GL_SCISSOR_TEST);
            glClear(GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
        }
        if (verifyPlayerProjection && storePanel != nullptr) {
            // Independent oracle from CBrother::DrawUI :136337-136357.
            // Compare the actual GL viewport + MVP, not just a helper's return.
            const auto &brother = equippedPreview->weapon->brother;
            const MeshBounds &torso = brother.GetTorso().GetAnimation().GetMesh()->GetBounds();
            const float expectedScale = storePanel->height / std::abs(torso.maxZ - torso.minZ);
            const float expectedX = storePanel->x + static_cast<int>(storePanel->width) / 2;
            const float expectedY = static_cast<float>(static_cast<int>(storePanel->y - torso.centerZ * expectedScale +
                storePanel->height + storePanel->height * 0.5f));
            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);
            const float actualX = (viewport[0] + (model[3] + 1) * viewport[2] / 2) * kMenuWidth / width;
            const float actualY = (height - viewport[1] - (model[7] + 1) * viewport[3] / 2) * kMenuHeight / height;
            const float actualScale = std::abs(model[0]) * viewport[2] * kMenuWidth / (2 * width);
            const bool matches = std::abs(actualX - expectedX) < 0.01f && std::abs(actualY - expectedY) < 0.01f &&
                std::abs(actualScale - expectedScale) < 0.01f && glIsEnabled(GL_SCISSOR_TEST) == GL_FALSE;
            std::printf("[store-player-check] origin=(%.3f,%.3f) expected=(%.3f,%.3f) scale=%.3f expected=%.3f clip=%u match=%u\n",
                actualX, actualY, expectedX, expectedY, actualScale, expectedScale, glIsEnabled(GL_SCISSOR_TEST), matches);
            if (!matches) { return false; }
        }
        DrawPlayer(*equippedPreview, imageProgram, model);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, width, height);
        return true;
    }

    /** CMenuMeshPlayer::Update observes configuration only after native 3. */
    void GameMenu::AdvancePlayerPreview(int deltaMs) {
        CBrother &brother = equippedPreview->weapon->brother;
        brother.UpdateUI(deltaMs);
        previewAudio.Update();
        PlayPreviewSounds(brother.GetTorso().TakeSounds());
        PlayPreviewSounds(brother.GetLegs().TakeSounds());
        if (brother.TakeWeaponSwap() && previewSwapPending) {
            previewGunSlot = 1 - previewGunSlot;
            SelectPlayerUIWeapon(*equippedPreview, previewGunSlot == previewPrimarySlot);
            previewSwapPending = false;
            previewSlotChanged = true;
            std::printf("[player-ui] native-swap slot=%u state=%d torso-preserved=1\n", previewGunSlot, brother.GetStateId());
        }
    }

    void GameMenu::PlayPreviewSounds(const std::vector<MoveSoundRef> &sounds) {
        // UpdateUI :137574 uses direct WAV ordinals, not SoundEffect templates.
        for (const auto &sound : sounds) {
            const std::uint64_t key = (static_cast<std::uint64_t>(sound.packHash) << 32) | sound.localIndex;
            if (!previewAudio.HasSound(key)) {
                std::vector<std::uint8_t> bytes;
                if (!resourceTables->ReadSectionResource(sound.packHash, GameSection::Wav, sound.localIndex, bytes) ||
                    !previewAudio.Load(key, bytes)) {
                    std::printf("[player-ui-audio] failed WAV=%08x:%u\n", sound.packHash, sound.localIndex);
                    continue;
                }
            }
            if (!previewAudio.Play(key)) {
                std::printf("[player-ui-audio] playback failed WAV=%08x:%u\n", sound.packHash, sound.localIndex);
                continue;
            }
            ++previewSoundCount;
            std::printf("[player-ui-audio] WAV=%08x:%u\n", sound.packHash, sound.localIndex);
        }
    }
}
