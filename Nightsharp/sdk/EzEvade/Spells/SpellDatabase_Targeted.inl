// ============================================================================
// SpellDatabase_Targeted.inl — Targeted (point-click) spell entries
// Ported from SpellDatabase.lua (SpellDatabase.Targeted table)
// Patch: 26.5 (2026-03-03)
//
// These are non-skillshot spells that still need to be tracked/dodged.
// Format: S(Champion, SpellName, DisplayName, IsMissile, IsCC, DangerLevel, EvadePct)
// ============================================================================

// ===== Aatrox =====
S("Aatrox","AatroxWTether","W Tether",false,true,4, 0)

// ===== Ahri =====
S("Ahri","AhriW","W",false,false,1, 0)
S("Ahri","AhriR","R",false,false,1, 0)

// ===== Akshan =====
S("Akshan","AkshanE","E",true, false,2, 0)
S("Akshan","AkshanR","R",true, false,5, 0)

// ===== Alistar =====
S("Alistar","AlistarW","W",false,true,5, 0)

// ===== Anivia =====
S("Anivia","AniviaE","E",true,false,3, 0)

// ===== Annie =====
S("Annie","AnnieQ1","Q",       true, false,1, 0)
S("Annie","AnnieQ2","Q Stun",  false,true, 5, 100)

// ===== Aphelios =====
S("Aphelios","ApheliosAA","AA Calibrum",true, false,2, 0)
S("Aphelios","ApheliosQ", "Q Gravitum",false,true, 5, 100)

// ===== Ashe =====
S("Ashe","AsheQ","Q",true,false,1, 0)

// ===== Blitzcrank =====
S("Blitzcrank","BlitzcrankE","E",false,true,5, 100)

// ===== Brand =====
S("Brand","BrandE1","E",false,false,1, 0)
S("Brand","BrandR1","R",true, false,4, 0)

// ===== Briar =====
S("Briar","BriarQ","Q",false,true,5, 100)

// ===== Caitlyn =====
S("Caitlyn","CaitlynP","P",true, false,3, 0)
S("Caitlyn","CaitlynR","R",true, false,5, 100)

// ===== Camille =====
S("Camille","CamilleQ1","Q1",false,false,2, 0)
S("Camille","CamilleQ2","Q2",false,false,4, 0)
S("Camille","CamilleR", "R", false,true, 5, 0)

// ===== Cassiopeia =====
S("Cassiopeia","CassiopeiaE","E",true,false,2, 0)

// ===== Chogath =====
S("Chogath","ChogathR","R",false,false,5, 100)

// ===== Darius =====
S("Darius","DariusW","W",false,false,3, 0)
S("Darius","DariusR","R",false,false,5, 0)

// ===== Diana =====
S("Diana","DianaE","E",false,false,3, 0)

// ===== Ekko =====
S("Ekko","EkkoE","E",false,false,3, 100)

// ===== Elise =====
S("Elise","EliseQ1","Q Human",true, false,3, 0)
S("Elise","EliseQ2","Q Spider",false,false,4, 0)

// ===== Evelynn =====
S("Evelynn","EvelynnQ2","Q2",false,false,2, 0)
S("Evelynn","EvelynnW", "W", true, true, 5, 0)
S("Evelynn","EvelynnE1","E", false,false,3, 0)

// ===== FiddleSticks =====
S("FiddleSticks","FiddleSticksQ","Q",true,true,5, 100)

// ===== Fiora =====
S("Fiora","FioraE1","E",false,false,2, 0)
S("Fiora","FioraE2","E2",false,false,3, 0)

// ===== Fizz =====
S("Fizz","FizzQ","Q",false,false,4, 100)

// ===== Galio =====
S("Galio","GalioP","P",false,false,3, 0)
S("Galio","GalioW","W",false,true, 5, 0)

// ===== Gangplank =====
S("Gangplank","GangplankQ","Q",true,false,3, 0)

// ===== Garen =====
S("Garen","GarenQ","Q",false,false,2, 100)
S("Garen","GarenR","R",false,false,5, 100)

// ===== Gragas =====
S("Gragas","GragasW","W",false,false,2, 0)

// ===== Hecarim =====
S("Hecarim","HecarimE","E",false,true,5, 100)

// ===== Illaoi =====
S("Illaoi","IllaoiW","W",false,false,3, 0)

// ===== Irelia =====
S("Irelia","IreliaQ","Q",false,false,3, 0)

// ===== Janna =====
S("Janna","JannaW","W",true,true,3, 0)

// ===== JarvanIV =====
S("JarvanIV","JarvanIVP","P",false,false,2, 0)

// ===== Jax =====
S("Jax","JaxQ","Q",false,false,3, 0)
S("Jax","JaxW","W",false,false,2, 0)
S("Jax","JaxE","E",false,true, 5, 100)
S("Jax","JaxR","R",false,false,3, 0)

// ===== Jayce =====
S("Jayce","JayceQ1","Q Melee",false,false,3, 0)
S("Jayce","JayceE1","E Melee",false,true, 5, 0)

// ===== Jhin =====
S("Jhin","JhinQ1","Q",true,false,1, 0)

// ===== KSante =====
S("KSante","KSanteR","R",false,true,5, 0)

// ===== Kaisa =====
S("Kaisa","KaisaP","P",true, false,3, 0)
S("Kaisa","KaisaQ","Q",true, false,1, 0)

// ===== Karma =====
S("Karma","KarmaW1","W",      false,true, 5, 100)
S("Karma","KarmaW2","W Renew",false,true, 5, 100)

// ===== Karthus =====
S("Karthus","KarthusR","R",false,false,5, 0)

// ===== Katarina =====
S("Katarina","KatarinaE","E",false,false,3, 0)

// ===== Kayle =====
S("Kayle","KayleR","R",false,false,5, 0)

// ===== Lee Sin =====
S("LeeSin","LeeSinQ2","Q2",false,true,4, 100)
S("LeeSin","LeeSinR", "R", false,true,4, 100)

// ===== Leona =====
S("Leona","LeonaQ","Q",false,true,4, 100)

// ===== Lissandra =====
S("Lissandra","LissandraREnemy","R",false,true,5, 100)

// ===== Lulu =====
S("Lulu","LuluW","W",false,true,4, 100)

// ===== Malzahar =====
S("Malzahar","MalzaharRMissile","R",false,true,5, 0)

// ===== MasterYi =====
S("MasterYi","MasterYiQ","Q",false,false,3, 0)

// ===== MonkeyKing (Wukong) =====
S("MonkeyKing","MonkeyKingQ","Q",false,false,3, 0)
S("MonkeyKing","MonkeyKingE","E",false,false,3, 0)

// ===== Mordekaiser =====
S("Mordekaiser","MordekaiserRMissile","R",false,false,5, 0)

// ===== Nasus =====
S("Nasus","NasusQ","Q",false,false,3, 0)
S("Nasus","NasusW","W Wither",false,true,5, 100)

// ===== Nautilus =====
S("Nautilus","NautilusQ","Q",true,true,4, 100)
S("Nautilus","NautilusRWrapper","R",false,true,5, 100)

// ===== Nocturne =====
S("Nocturne","NocturneQ","Q",false,false,3, 0)
S("Nocturne","NocturneR","R",false,false,5, 0)

// ===== Olaf =====
S("Olaf","OlafQ","Q",true,true,4, 0)

// ===== Pantheon =====
S("Pantheon","PantheonW","W",false,false,3, 0)

// ===== Renekton =====
S("Renekton","RenektonW","W",false,true,4, 0)

// ===== Sejuani =====
S("Sejuani","SejuaniR","R",true,true,5, 100)

// ===== Talon =====
S("Talon","TalonQ","Q",false,false,3, 0)

// ===== Teemo =====
S("Teemo","TeemoQ","Q",true,true,4, 0)

// ===== Tristana =====
S("Tristana","TristanaE","E",true,false,3, 0)

// ===== Trundle =====
S("Trundle","TrundleR","R",false,false,5, 0)

// ===== Udyr =====
S("Udyr","UdyrQ","Q",false,false,3, 0)
S("Udyr","UdyrE","E",false,true,5, 100)

// ===== Vayne =====
S("Vayne","VayneQ", "Q",  true, false,3, 0)
S("Vayne","VayneQR","Q R",true, false,4, 0)
S("Vayne","VayneE", "E",  true, true, 5, 100)

// ===== Veigar =====
S("Veigar","VeigarR","R",true,false,5, 40)

// ===== Vex =====
S("Vex","VexR1","R1",true,true,4, 100)

// ===== Vi =====
S("Vi","ViR","R",false,true,5, 100)

// ===== XinZhao =====
S("XinZhao","XinZhaoQ", "Q", false,false,1, 0)
S("XinZhao","XinZhaoQ3","Q3",false,true, 5, 100)
S("XinZhao","XinZhaoE", "E", false,false,3, 0)

// ===== Zed =====
S("Zed","ZedR", "R",false,false,5, 0)
S("Zed","ZedR2","R2",false,false,5, 40)

// ===== Zilean =====
S("Zilean","ZileanE","E",false,true,5, 100)
