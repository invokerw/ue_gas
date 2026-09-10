# HUD 示意美术

`ranger-portrait.png` 是 2026-09-09 通过内置 imagegen 生成的原创游侠示意头像；导入为 `/Game/Combat/Demo/UI/Art/T_HUDRangerPortrait`，仅用作本地 HUD 美术，不改战斗定义。

生成提示词：

> Use case: stylized-concept. Asset type: square portrait texture for a small fantasy MOBA HUD, displayed at about 144 by 154 UI units. Primary request: an original hooded ranger hero portrait, head and shoulders, painterly high-end game illustration. Muted teal cloak with weathered bronze fastening, human ranger with focused calm expression, face on the left-center, looking slightly right. Restrained dark blue and forest green palette with cool rim light and a small warm highlight on face. Portrait fills the image edges, no frame, no text, no icons, no UI. Keep right third and bottom-left corner darker and visually quiet because game stats and a level badge overlay those areas. Strong clear shapes that read well when tiny. Original character, no copying any existing game character. Square 1024 by 1024 composition.

正式美术接入时，在 `WBP_CombatHUD` 的“定义图标”中替换 `CombatUnit:ranged_combat_player` 对应纹理即可。
