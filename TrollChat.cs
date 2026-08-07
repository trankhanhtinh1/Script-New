
using System;
using System.Collections.Generic;
using System.Linq;
using EnsoulSharp;
using EnsoulSharp.SDK;
using EnsoulSharp.SDK.Utility;
using EnsoulSharp.SDK.MenuUI;
using SharpDX;
using EnsoulSharp.SDK.Rendering;
using EnsoulSharp.SDK.Core;
using Color = System.Drawing.Color;


namespace AIO7UP.Champions
{
    internal class TrollChat
    {
        private static Spell Q1, Q2, Q3, W, E, R, R1;
        private static Menu mainMenu;
        public static void OnGameLoad()
        {
            mainMenu = new Menu("TrollChat", "Troll Chat", true);
            mainMenu.Add(new MenuBool("Enabled", "Enabled"));
            //mainMenu.Add(new MenuBool("MouseScrollEnabled", "Change Enable/Disable Status with Mouse Scroll", false));
            //mainMenu.Add(new MenuBool("DrawStatus", "Draw Status", false));
            mainMenu.Add(new MenuKeyBind("PrintGG", "Print GG", Keys.N, KeyBindType.Press));
            mainMenu.Add(new MenuKeyBind("PrintWP", "Print WP", Keys.H, KeyBindType.Press));
            mainMenu.Add(new MenuKeyBind("PrintMiddleFinger", "Print Middle Finger", Keys.Z, KeyBindType.Press));
            mainMenu.Add(new MenuKeyBind("PrintXD", "Print XD", Keys.G, KeyBindType.Press));
            mainMenu.Add(new MenuKeyBind("PrintDick", "Prind Dick", Keys.J, KeyBindType.Press));
            mainMenu.Attach();
            //Drawing.OnDraw += DrawingOnOnDraw;
            Game.OnUpdate += GameOnOnTick;
            //Game.OnWndProc += GameOnOnWndProc;

        }
        
        /*private static void DrawingOnOnDraw(EventArgs args)
        {
            DrawText(ObjectManager.Player.Position, 0, +50, mainMenu["Enabled"].GetValue<MenuBool>().Enabled ? Color.White : Color.Red,
                mainMenu["Enabled"].GetValue<MenuBool>().Enabled ? "Troll Chat Enabled" : "Troll Chat Disabled", mainMenu["DrawStatus"].GetValue<MenuBool>().Enabled);
        }
        public static void DrawText(Vector3 position, float addPosX, float addPosY, Color color, string text, bool checkValue)
        {
            if (checkValue)
            {
                var pos = Drawing.WorldToScreen(position);
                Drawing.DrawText(pos.X + addPosX, pos.Y + addPosY, color, text);
            }
        }*/
        /*private static void GameOnOnWndProc(GameWndEventArgs args)
        {
            if (args.Msg != 0x20a || !mainMenu["MouseScrollEnabled"].GetValue<MenuBool>().Enabled)
                return;
            ChangeEnabledStatus();
        }
        private static void ChangeEnabledStatus()
        {
            if (mainMenu["Enabled"].GetValue<MenuBool>().Enabled)
            {
                Config.SetMenuBool("Settings", "Enabled", false);
            }
            else
            {
                Config.SetMenuBool("Settings", "Enabled", true);
            }
        }*/
        private static void GameOnOnTick(EventArgs args)
        {
            if (mainMenu["Enabled"].GetValue<MenuBool>().Enabled)
                {
                if (mainMenu["PrintDick"].GetValue<MenuKeyBind>().Active)
                {
                        PrintDick();
                }
                if (mainMenu["PrintGG"].GetValue<MenuKeyBind>().Active)
                {

                        PrintGG();

                }
                if (mainMenu["PrintMiddleFinger"].GetValue<MenuKeyBind>().Active)
                {

                        PrintMiddleFinger();

                }
                if (mainMenu["PrintWP"].GetValue<MenuKeyBind>().Active)
                {

                        PrintWP();

                }
                if (mainMenu["PrintXD"].GetValue<MenuKeyBind>().Active)
                {

                        PrintXD();

                }
            }
        }
        public static void PrintXD()
        {
            Game.Say("##       ##   ########  ", true);
            Game.Say(".##     ##    ##            ## ", true);
            Game.Say("..##   ##     ##            ## ", true);
            Game.Say(".....###        ##            ## ", true);
            Game.Say("..##   ##     ##            ## ", true);
            Game.Say(".##     ##    ##            ## ", true);
            Game.Say("##       ##   ########  ", true);

        }
        public static void PrintMiddleFinger()
        {
            Game.Say("....................../´¯/) ", true);
            Game.Say("....................,/¯../ ", true);
            Game.Say(".................../..../ ", true);
            Game.Say("............./´¯/'...'/´¯¯`·¸ ", true);
            Game.Say("........../'/.../..../......./¨¯\\ ", true);
            Game.Say("........('(...´...´.... ¯~/'...') ", true);
            Game.Say(".........\\.................'...../ ", true);
            Game.Say("..........''...\\.......... _.·´ ", true);
        }

        public static void PrintWP()
        {
            Game.Say("                            ", true);
            Game.Say("#      #    ###### ", true);
            Game.Say("#  #  #   #          #", true);
            Game.Say("#  #  #   #          #", true);
            Game.Say("#  #  #   ###### ", true);
            Game.Say("#  #  #   #     ", true);
            Game.Say("#  #  #   #     ", true);
            Game.Say(" ## ##    #     ", true);
        }
        public static void PrintGG()
        {
            Game.Say("                            ", true);
            Game.Say("     #####      ##### ", true);
            Game.Say(" #               #        ", true);
            Game.Say(" #               #      ", true);
            Game.Say(" #  ####   #  ####", true);
            Game.Say(" #         #   #         #", true);
            Game.Say(" #         #   #         #", true);
            Game.Say("     #####      ##### ", true);

        }

        public static void PrintDick()
        {
            Game.Say(".  ___", true);
            Game.Say(". //     7     ", true);
            Game.Say("(_,_  / \\", true);
            Game.Say(". \\        \\     ", true);
            Game.Say(".  \\        \\", true);
            Game.Say(". _\\        \\__ ", true);
            Game.Say(".(       \\       )", true);
            Game.Say(". \\____\\___/", true);
        }


    }
}