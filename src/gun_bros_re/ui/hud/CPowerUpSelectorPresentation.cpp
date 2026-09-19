/** Selector completion chain: SetState :185697, Update :186581.
 * Menu uses chapter 3; CMenuMovieControl::ChangeMode :141357 hides
 * item layouts by reversing chapter 0. Resource bounds come from ui_movie.bt.
 */
#include "gun_bros_re/ui/hud/CPowerUpSelector.h"
#include "gun_bros_re/ui/hud/CInputPad.h"

bool CPowerUpSelector::BeginPowerupPresentation(bool fromSelector) {
    m_presentationHasContent = m_selectorBound;
    m_presentingPowerup = fromSelector;
    m_powerupFrameVisible = fromSelector;
    m_powerupItemsVisible = fromSelector;
    m_presentationState = 0;
    if (!fromSelector) { return true; }
    if (m_ownedResources && !m_presentationResourcesReady) {
        CResPackTOC &core = *m_resources.m_toc->GetPack(m_resources.m_toc->GetCorePackIndex());
        if (!m_resources.m_movies.Init(core, core)) { return false; }
        m_presentationResourcesReady = true;
    }
    const auto *movie = m_resources.m_movies.GetMovie(m_resources.m_movies.Ordinal("GLU_MOVIE_POWERUP_MENU_NEW"));
    if (movie == nullptr) { return false; }
    m_powerupMenu.Bind(*movie);
    m_powerupMenu.SetLoop(true);
    return m_powerupMenu.SetChapter(2);
}

void CPowerUpSelector::EndPowerupPresentation() {
    m_powerupMenu.Cancel();
    m_powerupItems.Cancel();
    if (m_inputPad != nullptr) { m_inputPad->CancelPowerupAnimation(*m_powerup); }
    if (m_presentingPowerup) { m_selectorBound = false; }
    m_presentingPowerup = false;
    m_powerupFrameVisible = false;
    m_powerupItemsVisible = false;
    m_presentationState = 0;
}

void CPowerUpSelector::FinishPowerupPresentation() {
    // Instant-use exports may Exit without requesting any selector animation.
    // Only an already requested close should outlive the powerup itself.
    if (m_presentationState == 4 || m_presentationState == 6 || m_presentationState == 7) { return; }
    EndPowerupPresentation();
}

bool CPowerUpSelector::HideOnlyItems() {
    const char *name = "GLU_MOVIE_POWER_UP_LAYOUT";
    if (m_matchGuns) { name = "GLU_MOVIE_GUN_LAYOUT"; }
    const auto *movie = m_resources.m_movies.GetMovie(m_resources.m_movies.Ordinal(name));
    if (movie == nullptr) { return false; }
    m_powerupItems.Bind(*movie);
    m_powerupItems.SetReverse(true);
    if (!m_powerupItems.SetChapter(0)) { return false; }
    m_presentationState = 5;
    return true;
}

bool CPowerUpSelector::HideSelector() {
    m_powerupMenu.SetLoop(false);
    if (!m_powerupMenu.SetChapter(3)) { return false; }
    m_presentationState = 6;
    return true;
}

bool CPowerUpSelector::Hide() {
    // Native Hide has no OnSelectorHidden callback. State 4 first hides
    // items, then state 7 closes the frame (unlike native 5 / state 6).
    if (m_presentationState == 5) {
        if (!HideSelector()) { return false; }
        m_presentationState = 7;
        return true;
    }
    if (!HideOnlyItems()) { return false; }
    m_presentationState = 4;
    return true;
}

bool CPowerUpSelector::RestoreInputPad() {
    if (m_inputPad != nullptr) { return m_inputPad->RestoreForPowerup(*m_powerup); }
    // A peer/research selector has no input pad to animate. As in native 13
    // when no component needs restoration, completion is immediate.
    m_powerup->OnInputPadAnimationComplete();
    return true;
}

void CPowerUpSelector::UpdatePowerupPresentation(unsigned deltaMs) {
    if (!m_presentingPowerup || deltaMs == 0) { return; }
    m_powerupMenu.Update(deltaMs);
    if (m_presentationState == 4 || m_presentationState == 5) {
        m_powerupItems.Update(deltaMs);
        if (m_powerupItems.TakeCompletion()) {
            m_powerupItemsVisible = false;
            if (m_presentationState == 5) { m_powerup->OnItemsHidden(); }
            else {
                if (!HideSelector()) { ++failures; }
                m_presentationState = 7;
            }
        }
    } else if (m_presentationState == 6 || m_presentationState == 7) {
        if (m_powerupMenu.TakeCompletion()) {
            const bool notify = m_presentationState == 6;
            m_powerupFrameVisible = false;
            m_presentationState = 8;
            if (notify) { m_powerup->OnSelectorHidden(); }
        }
    }
}

bool CPowerUpSelector::DrawPowerupPresentation() {
    if (!HasPowerupFrame()) { return true; }
    if (m_presentationHasContent) { return DrawSelector(m_presentationSnapshot); }
    // Isolated research callers can render the same frame without inventory UI.
    return m_resources.m_movies.Draw(m_resources.m_movies.Ordinal("GLU_MOVIE_POWERUP_MENU_NEW"), m_powerupMenu.GetTime());
}
