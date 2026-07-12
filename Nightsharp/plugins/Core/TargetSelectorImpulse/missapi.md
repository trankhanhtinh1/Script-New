## TargetSelectorImpulse

### Render.OnRenderMouseOvers / AIHeroClient.Glow
- **C# gốc:** `NewTargetSelector.cs:52-65` — `Render.OnRenderMouseOvers += OnLight` and `SelectedTarget.Glow(Purple, 5, 1)`.
- **Dùng để:** highlight selected target during the mouse-over render phase.
- **Đã search:** `Glow`, `RenderMouseOver`, `MouseOvers`, `Highlight` in `NightSharp/sdk`, `NightSharp/plugins`, and `Old` — no equivalent SDK event/object API exists.
- **Trạng thái:** BLOCKED — requires a verified SDK glow/render-mouse-over API.
- **Chỗ comment trong code:** `TargetSelectorImpulse.h`, function `OnLight`.
