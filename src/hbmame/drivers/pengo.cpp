// license:GPL_2.0
// copyright-holders:Robbbert

#include "emu.h"
#include "includes/puckman.h"

#include "cpu/z80/z80.h"
#include "machine/74259.h"
#include "machine/gen_latch.h"
#include "machine/segacrpt_device.h"
#include "sound/ay8910.h"
#include "screen.h"
#include "speaker.h"


class pengo_state : public puckman_state
{
public:
	pengo_state(const machine_config &mconfig, device_type type, const char *tag)
		: puckman_state(mconfig, type, tag)
		, m_decrypted_opcodes(*this, "decrypted_opcodes")
		, m_latch(*this, "latch")
	{ }

	void jrpacmbl(machine_config &config);
	void pengoe(machine_config &config);
	void pengou(machine_config &config);
	void pengo(machine_config &config);

	void init_penta();

private:
	DECLARE_WRITE_LINE_MEMBER(coin_counter_1_w);
	DECLARE_WRITE_LINE_MEMBER(coin_counter_2_w);
	DECLARE_WRITE_LINE_MEMBER(irq_mask_w);
	DECLARE_WRITE_LINE_MEMBER(vblank_irq);

	void decode_penta(int end, int nodecend);

	optional_shared_ptr<uint8_t> m_decrypted_opcodes;
	optional_device<ls259_device> m_latch;
	void decrypted_opcodes_map(address_map &map);
	void jrpacmbl_map(address_map &map);
	void pengo_map(address_map &map);
};




/*************************************
 *
 *  Constants
 *
 *************************************/

#define MASTER_CLOCK        (18432000)

#define PIXEL_CLOCK         (MASTER_CLOCK/3)

/* H counts from 128->511, HBLANK starts at 128+16=144 and ends at 128+64+32+16=240 */
#define HTOTAL              (384)
#define HBEND               (0)     /*(96+16)*/
#define HBSTART             (288)   /*(16)*/

#define VTOTAL              (264)
#define VBEND               (0)     /*(16)*/
#define VBSTART             (224)   /*(224+16)*/



/*************************************
 *
 *  Main CPU memory handlers
 *
 *************************************/

WRITE_LINE_MEMBER(pengo_state::coin_counter_1_w)
{
	machine().bookkeeping().coin_counter_w(0, state);
}

WRITE_LINE_MEMBER(pengo_state::coin_counter_2_w)
{
	machine().bookkeeping().coin_counter_w(1, state);
}

WRITE_LINE_MEMBER(pengo_state::irq_mask_w)
{
	m_irq_mask = state;
}

void pengo_state::pengo_map(address_map &map)
{
	map(0x0000, 0x7fff).rom();
	map(0x8000, 0x83ff).ram().w(FUNC(pengo_state::pacman_videoram_w)).share("videoram"); /* video and color RAM, scratchpad RAM, sprite codes */
	map(0x8400, 0x87ff).ram().w(FUNC(pengo_state::pacman_colorram_w)).share("colorram");
	map(0x8800, 0x8fef).ram().share("mainram");
	map(0x8ff0, 0x8fff).ram().share("spriteram");
	map(0x9000, 0x901f).w(m_namco_sound, FUNC(namco_device::pacman_sound_w));
	map(0x9020, 0x902f).writeonly().share("spriteram2");
	map(0x9000, 0x903f).portr("DSW1");
	map(0x9040, 0x907f).portr("DSW0");
	map(0x9040, 0x9047).w(m_latch, FUNC(ls259_device::write_d0));
	map(0x9070, 0x9070).w(m_watchdog, FUNC(watchdog_timer_device::reset_w));
	map(0x9080, 0x90bf).portr("IN1");
	map(0x90c0, 0x90ff).portr("IN0");
}


void pengo_state::decrypted_opcodes_map(address_map &map)
{
	map(0x0000, 0x7fff).rom().share("decrypted_opcodes");
	map(0x8800, 0x8fef).ram().share("mainram");
	map(0x8ff0, 0x8fff).ram().share("spriteram");
}


void pengo_state::jrpacmbl_map(address_map &map)
{
	map(0x0000, 0x7fff).rom();
	map(0x8000, 0x87ff).ram().w(FUNC(pengo_state::jrpacman_videoram_w)).share("videoram");
	map(0x8800, 0x8fef).ram();
	map(0x8ff0, 0x8fff).ram().share("spriteram");
	map(0x9000, 0x901f).w(m_namco_sound, FUNC(namco_device::pacman_sound_w));
	map(0x9020, 0x902f).writeonly().share("spriteram2");
	map(0x9030, 0x9030).w(FUNC(pengo_state::jrpacman_scroll_w));
	map(0x9040, 0x904f).portr("DSW");
	map(0x9040, 0x9047).w(m_latch, FUNC(ls259_device::write_d0));
	map(0x9070, 0x9070).w(m_watchdog, FUNC(watchdog_timer_device::reset_w));
	map(0x9080, 0x90bf).portr("P2");
	map(0x90c0, 0x90ff).portr("P1");
}


/*************************************
 *
 *  Port definitions
 *
 *************************************/

static INPUT_PORTS_START( pengo )
	PORT_START("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_4WAY
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_4WAY
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_4WAY
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_4WAY
	/* the coin input must stay low for no less than 2 frames and no more */
	/* than 9 frames to pass the self test check. */
	/* Moreover, this way we avoid the game freezing until the user releases */
	/* the "coin" key. */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_IMPULSE(2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 ) PORT_IMPULSE(2)
	/* Coin Aux doesn't need IMPULSE to pass the test, but it still needs it */
	/* to avoid the freeze. */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 ) PORT_IMPULSE(2)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )

	PORT_START("IN1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_4WAY PORT_COCKTAIL
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_4WAY PORT_COCKTAIL
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_4WAY PORT_COCKTAIL
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_4WAY PORT_COCKTAIL
	PORT_SERVICE_NO_TOGGLE(0x10, IP_ACTIVE_LOW)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_COCKTAIL

	PORT_START("DSW0")
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Bonus_Life ) )       PORT_DIPLOCATION("SW1:1")
	PORT_DIPSETTING(    0x00, "30000" )
	PORT_DIPSETTING(    0x01, "50000" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )      PORT_DIPLOCATION("SW1:2")
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )          PORT_DIPLOCATION("SW1:3")
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x18, 0x10, DEF_STR( Lives ) )            PORT_DIPLOCATION("SW1:4,5")
	PORT_DIPSETTING(    0x18, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x20, 0x20, "Rack Test (Cheat)" ) PORT_CODE(KEYCODE_F1)   PORT_DIPLOCATION("SW1:6")
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Difficulty ) )       PORT_DIPLOCATION("SW1:7,8")
	PORT_DIPSETTING(    0xc0, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Medium ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )

	PORT_START("DSW1")
	PORT_DIPNAME( 0x0f, 0x0c, DEF_STR( Coin_A ) )           PORT_DIPLOCATION("SW2:1,2,3,4")
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x09, "2 Coins/1 Credit 5/3" )
	PORT_DIPSETTING(    0x05, "2 Coins/1 Credit 4/3" )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0d, "1 Coin/1 Credit 5/6" )
	PORT_DIPSETTING(    0x03, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(    0x0b, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x07, "1 Coin/2 Credits 5/11" )
	PORT_DIPSETTING(    0x0f, "1 Coin/2 Credits 4/9" )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0xf0, 0xc0, DEF_STR( Coin_B ) )           PORT_DIPLOCATION("SW2:5,6,7,8")
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x90, "2 Coins/1 Credit 5/3" )
	PORT_DIPSETTING(    0x50, "2 Coins/1 Credit 4/3" )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0xd0, "1 Coin/1 Credit 5/6" )
	PORT_DIPSETTING(    0x30, "1 Coin/1 Credit 4/5" )
	PORT_DIPSETTING(    0xb0, "1 Coin/1 Credit 2/3" )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x70, "1 Coin/2 Credits 5/11" )
	PORT_DIPSETTING(    0xf0, "1 Coin/2 Credits 4/9" )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_6C ) )
INPUT_PORTS_END



static INPUT_PORTS_START( jrpacmbl )
	PORT_START("P1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_4WAY
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_4WAY
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_4WAY
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_4WAY
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("P2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_4WAY PORT_COCKTAIL
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_4WAY PORT_COCKTAIL
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_4WAY PORT_COCKTAIL
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_4WAY PORT_COCKTAIL
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("DSW")
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Coinage ) )          PORT_DIPLOCATION("SW1:1,2")
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x08, DEF_STR( Lives ) )            PORT_DIPLOCATION("SW1:3,4")
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x0c, "5" )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Bonus_Life ) )       PORT_DIPLOCATION("SW1:5,6")
	PORT_DIPSETTING(    0x00, "10000" )
	PORT_DIPSETTING(    0x10, "15000" )
	PORT_DIPSETTING(    0x20, "20000" )
	PORT_DIPSETTING(    0x30, "30000" )
	PORT_DIPNAME( 0x40, 0x40, "Rack Test (Cheat)" ) PORT_CODE(KEYCODE_F1)   PORT_DIPLOCATION("SW1:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )          PORT_DIPLOCATION("SW1:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
INPUT_PORTS_END



/*************************************
 *
 *  Graphics layouts
 *
 *************************************/

static const gfx_layout tilelayout =
{
	8,8,    /* 8*8 characters */
	RGN_FRAC(1,2),    /* 256 characters */
	2,  /* 2 bits per pixel */
	{ 0, 4 },   /* the two bitplanes for 4 pixels are packed into one byte */
	{ 8*8+0, 8*8+1, 8*8+2, 8*8+3, 0, 1, 2, 3 }, /* bits are packed in groups of four */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8    /* every char takes 16 bytes */
};


static const gfx_layout spritelayout =
{
	16,16,  /* 16*16 sprites */
	RGN_FRAC(1,2),  /* 64 sprites */
	2,  /* 2 bits per pixel */
	{ 0, 4 },   /* the two bitplanes for 4 pixels are packed into one byte */
	{ 8*8, 8*8+1, 8*8+2, 8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3,
			24*8+0, 24*8+1, 24*8+2, 24*8+3, 0, 1, 2, 3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8    /* every sprite takes 64 bytes */
};


static GFXDECODE_START( gfx_pengo )
	GFXDECODE_ENTRY( "gfx1", 0x0000, tilelayout,   0, 128 )
	GFXDECODE_ENTRY( "gfx1", 0x2000, spritelayout, 0, 128 )
GFXDECODE_END


/*************************************
 *
 *  Machine drivers
 *
 *************************************/

WRITE_LINE_MEMBER(pengo_state::vblank_irq)
{
	if (state && m_irq_mask)
		m_maincpu->set_input_line(0, HOLD_LINE);
}

void pengo_state::pengo(machine_config &config)
{
	/* basic machine hardware */
	Z80(config, m_maincpu, MASTER_CLOCK/6);
	m_maincpu->set_addrmap(AS_PROGRAM, &pengo_state::pengo_map);
	m_maincpu->set_addrmap(AS_OPCODES, &pengo_state::decrypted_opcodes_map);

	LS259(config, m_latch); // U27
	m_latch->q_out_cb<0>().set(FUNC(pengo_state::irq_mask_w));
	m_latch->q_out_cb<1>().set("namco", FUNC(namco_device::sound_enable_w));
	m_latch->q_out_cb<2>().set(FUNC(pengo_state::pengo_palettebank_w));
	m_latch->q_out_cb<3>().set(FUNC(pengo_state::flipscreen_w));
	m_latch->q_out_cb<4>().set(FUNC(pengo_state::coin_counter_1_w));
	m_latch->q_out_cb<5>().set(FUNC(pengo_state::coin_counter_2_w));
	m_latch->q_out_cb<6>().set(FUNC(pengo_state::pengo_colortablebank_w));
	m_latch->q_out_cb<7>().set(FUNC(pengo_state::pengo_gfxbank_w));

	WATCHDOG_TIMER(config, m_watchdog);

	/* video hardware */
	GFXDECODE(config, m_gfxdecode, m_palette, gfx_pengo);
	PALETTE(config, m_palette, FUNC(pengo_state::pacman_palette), 128 * 4, 32);

	screen_device &screen(SCREEN(config, "screen", SCREEN_TYPE_RASTER));
	screen.set_raw(PIXEL_CLOCK, HTOTAL, HBEND, HBSTART, VTOTAL, VBEND, VBSTART);
	screen.set_screen_update(FUNC(pengo_state::screen_update_pacman));
	screen.set_palette(m_palette);
	screen.screen_vblank().set(FUNC(pengo_state::vblank_irq));

	MCFG_VIDEO_START_OVERRIDE(pengo_state,pengo)

	/* sound hardware */
	SPEAKER(config, "mono").front_center();

	NAMCO(config, m_namco_sound, MASTER_CLOCK/6/32);
	m_namco_sound->set_voices(3);
	m_namco_sound->add_route(ALL_OUTPUTS, "mono", 1.0);
}

void pengo_state::pengou(machine_config &config)
{
	pengo(config);
	m_maincpu->set_addrmap(AS_OPCODES, address_map_constructor());
}

void pengo_state::pengoe(machine_config &config)
{
	pengo(config);
	sega_315_5010_device &maincpu(SEGA_315_5010(config.replace(), m_maincpu, MASTER_CLOCK/6));
	maincpu.set_addrmap(AS_PROGRAM, &pengo_state::pengo_map);
	maincpu.set_addrmap(AS_OPCODES, &pengo_state::decrypted_opcodes_map);
	maincpu.set_decrypted_tag(":decrypted_opcodes");
}

void pengo_state::jrpacmbl(machine_config &config)
{
	pengo(config);

	/* basic machine hardware */
	m_maincpu->set_addrmap(AS_PROGRAM, &pengo_state::jrpacmbl_map);
	m_maincpu->set_addrmap(AS_OPCODES, address_map_constructor());

	m_latch->q_out_cb<4>().set(FUNC(pengo_state::jrpacman_bgpriority_w));
	m_latch->q_out_cb<5>().set(FUNC(pengo_state::jrpacman_spritebank_w));
	m_latch->q_out_cb<7>().set(FUNC(pengo_state::jrpacman_charbank_w));

	MCFG_VIDEO_START_OVERRIDE(pengo_state,jrpacman)
}


/*************************************
 *
 *  ROM definitions
 *
 *************************************/

/*
Known to exist but not dumped:
  Pengo using SEGA 315-5007 CPU, EPROMs numbers EPR-1701 through EPR-1708 (with EPR-1701 & EPR-1708 as A rev)
  Sega game ID# 834-5078 PENGO
*/

ROM_START( pengo )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "epr-1689c.ic8",  0x0000, 0x1000, CRC(f37066a8) SHA1(0930de17a763a527057f60783a92662b09554426) )
	ROM_LOAD( "epr-1690b.ic7",  0x1000, 0x1000, CRC(baf48143) SHA1(4c97529e61eeca5d94938b1dfbeac41bf8cbaf7d) )
	ROM_LOAD( "epr-1691b.ic15", 0x2000, 0x1000, CRC(adf0eba0) SHA1(c8949fbdbfe5023ee17a789ef60205e834a76c81) )
	ROM_LOAD( "epr-1692b.ic14", 0x3000, 0x1000, CRC(a086d60f) SHA1(7079769d14dfe3873ffe29623ba0a93413706c6d) )
	ROM_LOAD( "epr-1693b.ic21", 0x4000, 0x1000, CRC(b72084ec) SHA1(c0508951c2ad8dc31481be8b3bfee2063e3fb0d7) )
	ROM_LOAD( "epr-1694b.ic20", 0x5000, 0x1000, CRC(94194a89) SHA1(7b47aec61593efd758e2a031f72a854bb0ba8af1) )
	ROM_LOAD( "epr-5118b.ic32", 0x6000, 0x1000, CRC(af7b12c4) SHA1(207ed466546f40ca60a38031b83aef61446902e2) )
	ROM_LOAD( "epr-5119c.ic31", 0x7000, 0x1000, CRC(933950fe) SHA1(fec7236b3dee2ea6e39c68440a6d2d9e3f72675a) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "epr-1640.ic92",  0x0000, 0x1000, CRC(d7eec6cd) SHA1(e542bcc28f292be9a0a29d949de726e0b55e654a) )
	ROM_CONTINUE(               0x2000, 0x1000 )
	ROM_LOAD( "epr-1695.ic105", 0x1000, 0x1000, CRC(5bfd26e9) SHA1(bdec535e486b43a8f5550334beff423eeace10b2) )
	ROM_CONTINUE(               0x3000, 0x1000 )

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.ic78",    0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) )
	ROM_LOAD( "pr1634.ic88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) )

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.ic51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) )
	ROM_LOAD( "pr1636.ic70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )
ROM_END


ROM_START( pengo2 )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "ic8.2",          0x0000, 0x1000, CRC(e4924b7b) SHA1(44297658af8f8c884eba02b792346c5008137dfe) )
	ROM_LOAD( "ic7.2",          0x1000, 0x1000, CRC(72e7775d) SHA1(49e04178ee171f727dd023c019395679cfbad452) )
	ROM_LOAD( "ic15.2",         0x2000, 0x1000, CRC(7410ef1e) SHA1(7ed8e16c6ce401904c0da9758e2a405d7b9b451b) )
	ROM_LOAD( "ic14.2",         0x3000, 0x1000, CRC(55b3f379) SHA1(bc244f97132f0514adb3d6ceda8afbd45c1c587a) )
	ROM_LOAD( "epr-1693b.ic21", 0x4000, 0x1000, CRC(b72084ec) SHA1(c0508951c2ad8dc31481be8b3bfee2063e3fb0d7) )
	ROM_LOAD( "ic20.2",         0x5000, 0x1000, CRC(770570cf) SHA1(43ead8236f53d39041ffc21bdeef10b3a77ce7f2) )
	ROM_LOAD( "epr-5118b.ic32", 0x6000, 0x1000, CRC(af7b12c4) SHA1(207ed466546f40ca60a38031b83aef61446902e2) )
	ROM_LOAD( "ic31.2",         0x7000, 0x1000, CRC(669555c1) SHA1(50d5cf8022af6d6bd022235ab06015cb5c8aa433) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "epr-1640.ic92",  0x0000, 0x1000, CRC(d7eec6cd) SHA1(e542bcc28f292be9a0a29d949de726e0b55e654a) )
	ROM_CONTINUE(               0x2000, 0x1000 )
	ROM_LOAD( "epr-1695.ic105", 0x1000, 0x1000, CRC(5bfd26e9) SHA1(bdec535e486b43a8f5550334beff423eeace10b2) )
	ROM_CONTINUE(               0x3000, 0x1000 )

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.ic78",    0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) )
	ROM_LOAD( "pr1634.ic88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) )

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.ic51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) )
	ROM_LOAD( "pr1636.ic70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )
ROM_END


ROM_START( pengo2u ) /* Sega game ID# 834-5092 PENGO REV.A */
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "epr-5128.ic8",   0x0000, 0x1000, CRC(3dfeb20e) SHA1(a387b72501da77bf38b58619d2099083a0463e1f) )
	ROM_LOAD( "epr-5129.ic7",   0x1000, 0x1000, CRC(1db341bd) SHA1(d1c66bb9cf479e6960dbcd35c820097a81eaa555) )
	ROM_LOAD( "epr-5130.ic15",  0x2000, 0x1000, CRC(7c2842d5) SHA1(a8a568da68babd0ccb9f2cee4182fc01c3138494) )
	ROM_LOAD( "epr-5131a.ic14", 0x3000, 0x1000, CRC(6e3c1f2f) SHA1(2ee821b0f6e0f3cfeae7f5ff25a6e9bd977efce0) )
	ROM_LOAD( "epr-5132.ic21",  0x4000, 0x1000, CRC(95f354ff) SHA1(fdebc68a6d87f8ecdf52a57a34ae5ae844a13510) )
	ROM_LOAD( "epr-5133.ic20",  0x5000, 0x1000, CRC(0fdb04b8) SHA1(ed814d58318c1055e475ff678609d189727bf9b4) )
	ROM_LOAD( "epr-5134.ic32",  0x6000, 0x1000, CRC(e5920728) SHA1(0ac5ffdad7bdcb32e630b9582e1b1aaece5198c9) )
	ROM_LOAD( "epr-5135a.ic31", 0x7000, 0x1000, CRC(13de47ed) SHA1(332b484d47c9921ed93432755bb2d7a9d4628939) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "epr-1640.ic92",  0x0000, 0x1000, CRC(d7eec6cd) SHA1(e542bcc28f292be9a0a29d949de726e0b55e654a) )
	ROM_CONTINUE(               0x2000, 0x1000 )
	ROM_LOAD( "epr-1695.ic105", 0x1000, 0x1000, CRC(5bfd26e9) SHA1(bdec535e486b43a8f5550334beff423eeace10b2) )
	ROM_CONTINUE(               0x3000, 0x1000 )

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.ic78",    0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) )
	ROM_LOAD( "pr1634.ic88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) )

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.ic51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) )
	ROM_LOAD( "pr1636.ic70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )
ROM_END


ROM_START( pengo3u ) /*  Sega game ID# 834-5091 PENGO */
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "epr-5120.ic8",     0x0000, 0x1000, CRC(f01afb60) SHA1(1db732a17a9f79f8f1751f80c77889142928e41b) )
	ROM_LOAD( "epr-5121.ic7",     0x1000, 0x1000, CRC(2eb38353) SHA1(d351347f93a3ed01c8b5274ec19352dd611a8dd4) )
	ROM_LOAD( "epr-5122.ic15",    0x2000, 0x1000, CRC(c33400d7) SHA1(7b9617d22a9de8d3658abe34b5d2171ce37acc39) )
	ROM_LOAD( "epr-5123.ic14",    0x3000, 0x1000, CRC(6a85c6a2) SHA1(444acc08607c892bb20b3a02753169addf5b11de) )
	ROM_LOAD( "epr-5124.ic21",    0x4000, 0x1000, CRC(95f354ff) SHA1(fdebc68a6d87f8ecdf52a57a34ae5ae844a13510) )
	ROM_LOAD( "epr-5125.ic20",    0x5000, 0x1000, CRC(1a42310f) SHA1(fef20385299a709ee17ed16510ac5702bd5cc257) )
	ROM_LOAD( "epr-5126.ic32",    0x6000, 0x1000, CRC(e5920728) SHA1(0ac5ffdad7bdcb32e630b9582e1b1aaece5198c9) )
	ROM_LOAD( "epr-5127.ic31",    0x7000, 0x1000, CRC(a7d3d1d6) SHA1(20e4353208c3803d8879b25f821ea617e9a19cc4) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "epr-1640.ic92",  0x0000, 0x1000, CRC(d7eec6cd) SHA1(e542bcc28f292be9a0a29d949de726e0b55e654a) )
	ROM_CONTINUE(               0x2000, 0x1000 )
	ROM_LOAD( "epr-1695.ic105", 0x1000, 0x1000, CRC(5bfd26e9) SHA1(bdec535e486b43a8f5550334beff423eeace10b2) )
	ROM_CONTINUE(               0x3000, 0x1000 )

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.ic78",    0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) )
	ROM_LOAD( "pr1634.ic88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) )

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.ic51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) )
	ROM_LOAD( "pr1636.ic70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )
ROM_END


ROM_START( pengo4 ) /* Sega game ID# 834-5081 PENGO (REV.A of this set known to exist, but not currently dumped) */
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "epr-1738.ic8",    0x0000, 0x1000, CRC(68ba25ea) SHA1(ce937831b7b210b4a625514bd4e6b3a7a36d008e) )
	ROM_LOAD( "epr-1739.ic7",    0x1000, 0x1000, CRC(41e7b5b3) SHA1(d512d41ee3f5716070250e7ab63342e4fbf92875) )
	ROM_LOAD( "epr-1740.ic15",   0x2000, 0x1000, CRC(27f05f59) SHA1(c0d40328a7dff34f6b84c991d9c88b240e55b4f3) )
	ROM_LOAD( "epr-1741.ic14",   0x3000, 0x1000, CRC(27d93ec1) SHA1(925e59878342af58106d5b11ebb6c86cbb69ae91) )
	ROM_LOAD( "epr-1742.ic21",   0x4000, 0x1000, CRC(b72084ec) SHA1(c0508951c2ad8dc31481be8b3bfee2063e3fb0d7) )
	ROM_LOAD( "epr-1743.ic20",   0x5000, 0x1000, CRC(770570cf) SHA1(43ead8236f53d39041ffc21bdeef10b3a77ce7f2) )
	ROM_LOAD( "epr-1744.ic32",   0x6000, 0x1000, CRC(af7b12c4) SHA1(207ed466546f40ca60a38031b83aef61446902e2) )
	ROM_LOAD( "epr-1745.ic31",   0x7000, 0x1000, CRC(507e18b9) SHA1(e169e4c9c6350fb5e4020222dbcaa6f5ce41849c) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "epr-1640.ic92",  0x0000, 0x1000, CRC(d7eec6cd) SHA1(e542bcc28f292be9a0a29d949de726e0b55e654a) )
	ROM_CONTINUE(               0x2000, 0x1000 )
	ROM_LOAD( "epr-1695.ic105", 0x1000, 0x1000, CRC(5bfd26e9) SHA1(bdec535e486b43a8f5550334beff423eeace10b2) )
	ROM_CONTINUE(               0x3000, 0x1000 )

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.ic78",    0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) )
	ROM_LOAD( "pr1634.ic88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) )

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.ic51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) )
	ROM_LOAD( "pr1636.ic70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )
ROM_END

/*
A PCB (not dumped) with dated ROMs like the set below has been seen with later dates:
0 (scratched off)
1 OCT11'82
2 OCT11'82
3 OCT12'82
4 (scratched off)
5 OCT12'82
6 OCT13'82
7 (scratched off)

PCB labeled as 834-5081 PENGO REV.A
*/
ROM_START( pengo5 ) // Sega game ID# 834-5081 PENGO - PCB has an additional label Bally N.E.
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "0_oct6-82.ic8",    0x0000, 0x1000, CRC(43e45441) SHA1(e94a9f9971e57cd53fe425059a6cb7cadbd206f1) )
	ROM_LOAD( "1_oct11-82.ic7",   0x1000, 0x1000, CRC(30a52a90) SHA1(e5ff7e16f40b42e56847d63ecbf4a0793f510c42) )
	ROM_LOAD( "2_oct11-82.ic15",  0x2000, 0x1000, CRC(09783cc2) SHA1(793559c86c690837041e611107589b94ed5831ed) )
	ROM_LOAD( "3_oct6-82.ic14",   0x3000, 0x1000, CRC(452c80c9) SHA1(2432930b88b9b5e7acc19cdcac7262199545ac2a) )
	ROM_LOAD( "4_oct6-82.ic21",   0x4000, 0x1000, CRC(b72084ec) SHA1(c0508951c2ad8dc31481be8b3bfee2063e3fb0d7) ) /* == epr-1742.ic21 */
	ROM_LOAD( "5_oct11-82.ic20",  0x5000, 0x1000, CRC(770570cf) SHA1(43ead8236f53d39041ffc21bdeef10b3a77ce7f2) ) /* == epr-1743.ic20 */
	ROM_LOAD( "6_oct11-82.ic32",  0x6000, 0x1000, CRC(af7b12c4) SHA1(207ed466546f40ca60a38031b83aef61446902e2) ) /* == epr-1744.ic32 */
	ROM_LOAD( "7_oct11-82.ic31",  0x7000, 0x1000, CRC(1350ca0e) SHA1(40619973d69176b05fa160372306ad50693db021) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "epr-1640.ic92",  0x0000, 0x1000, CRC(d7eec6cd) SHA1(e542bcc28f292be9a0a29d949de726e0b55e654a) ) /* tiles (bank 1), not dumped for this set but same label */
	ROM_CONTINUE(               0x2000, 0x1000 ) /* sprites (bank 1)*/
	ROM_LOAD( "epr-1695.ic105", 0x1000, 0x1000, CRC(5bfd26e9) SHA1(bdec535e486b43a8f5550334beff423eeace10b2) ) /* tiles (bank 2), not dumped for this set but same label */
	ROM_CONTINUE(               0x3000, 0x1000 )

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.ic78",    0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) )
	ROM_LOAD( "pr1634.ic88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) )

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.ic51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) )
	ROM_LOAD( "pr1636.ic70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )
ROM_END

ROM_START( pengob )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "1",            0x0000, 0x2000, CRC(e04064db) SHA1(6689066b443807646894a357317f468bfc92368a) )
	ROM_LOAD( "2",            0x2000, 0x2000, CRC(75752424) SHA1(634e696a692c7245dfa5c5dfd4ce87755c2a90d4) )
	ROM_LOAD( "021_pn03.bin", 0x4000, 0x1000, CRC(7824e3ef) SHA1(3395bb537614de8da763d05f0d2e312145017e8f) ) // 3 (1/2)
	ROM_LOAD( "020_pn07.bin", 0x5000, 0x1000, CRC(377b9663) SHA1(35327dc0f0c19fa5a863aaf8d8f3bfcd2a5717a9) ) // 3 (2/2)
	ROM_LOAD( "032_pn04.bin", 0x6000, 0x1000, CRC(bfde44c1) SHA1(97e8a360ce09faa36d864d7020b1669a349867c6) ) // 4 (1/2)
	ROM_LOAD( "031_pn08.bin", 0x7000, 0x1000, CRC(64e8c30d) SHA1(aa50c21db2ac8361fc575f0785e2aae57f338564) ) // 4 (2/2)

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "5",              0x0000, 0x1000, CRC(1232437b) SHA1(7ec410a2a802514449ccb05684762c25f29556b0) )
	ROM_CONTINUE(               0x2000, 0x1000 )
	ROM_LOAD( "epr-1695.ic105", 0x1000, 0x1000, CRC(5bfd26e9) SHA1(bdec535e486b43a8f5550334beff423eeace10b2) )
	ROM_CONTINUE(               0x3000, 0x1000 )

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.ic78",    0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) )
	ROM_LOAD( "pr1634.ic88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) )

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.ic51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) )
	ROM_LOAD( "pr1636.ic70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )
ROM_END


ROM_START( penta )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "008_pn01.bin", 0x0000, 0x1000, CRC(22f328df) SHA1(ba13b8d20ccde995a158cf62b4bc48cb369a0788) )
	ROM_LOAD( "007_pn05.bin", 0x1000, 0x1000, CRC(15bbc7d3) SHA1(1823e3ba7388d3f4d9262e9b9cf70f123862c546) )
	ROM_LOAD( "015_pn02.bin", 0x2000, 0x1000, CRC(de82b74a) SHA1(301c1223dd0b111f8439affcb96b6e29106364ed) )
	ROM_LOAD( "014_pn06.bin", 0x3000, 0x1000, CRC(160f3836) SHA1(ff90c82d52ed0c2c17a7aeabc9401ee9d8cf4d2d) )
	ROM_LOAD( "021_pn03.bin", 0x4000, 0x1000, CRC(7824e3ef) SHA1(3395bb537614de8da763d05f0d2e312145017e8f) )
	ROM_LOAD( "020_pn07.bin", 0x5000, 0x1000, CRC(377b9663) SHA1(35327dc0f0c19fa5a863aaf8d8f3bfcd2a5717a9) )
	ROM_LOAD( "032_pn04.bin", 0x6000, 0x1000, CRC(bfde44c1) SHA1(97e8a360ce09faa36d864d7020b1669a349867c6) )
	ROM_LOAD( "031_pn08.bin", 0x7000, 0x1000, CRC(64e8c30d) SHA1(aa50c21db2ac8361fc575f0785e2aae57f338564) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "092_pn09.bin",   0x0000, 0x1000, CRC(6afeba9d) SHA1(cd723fb94aa90dbaac9a6fe085c0f4786d2fa092) )
	ROM_CONTINUE(               0x2000, 0x1000 )
	ROM_LOAD( "epr-1695.ic105", 0x1000, 0x1000, CRC(5bfd26e9) SHA1(bdec535e486b43a8f5550334beff423eeace10b2) )
	ROM_CONTINUE(               0x3000, 0x1000 )

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.ic78",    0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) )
	ROM_LOAD( "pr1634.ic88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) )

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.ic51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) )
	ROM_LOAD( "pr1636.ic70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )
ROM_END



ROM_START( jrpacmbl )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "jrpacpe-05.ic8",     0x0000, 0x1000, CRC(98049df4) SHA1(4ea022c8664dd9ec185f9d5990a548e867e5071f) )
	ROM_LOAD( "jrpacpe-01.ic7",     0x1000, 0x1000, CRC(b7a5cef8) SHA1(c315970f0dd698a1036df12502e3fe3ec7f81d53) )
	ROM_LOAD( "jrpacpe-06.ic6",     0x2000, 0x1000, CRC(ecf39785) SHA1(9e47f29f4cadb5d8fd3790c7e16c653fc0a96a88) )
	ROM_LOAD( "jrpacpe-02.ic5",     0x3000, 0x1000, CRC(c090145c) SHA1(918d32267379b99f898fdcd987b3a65f9ec4f088) )
	ROM_LOAD( "jrpacpe-07.ic4",     0x4000, 0x1000, CRC(659b9956) SHA1(5576d4d95ced804e8abdd870662574bfdd6df18f) )
	ROM_LOAD( "jrpacpe-03.ic3",     0x5000, 0x1000, CRC(0ebcfac9) SHA1(4da01169768e35601e04df7004ae7496f08a709a) )
	ROM_LOAD( "jrpacpe-08.ic2",     0x6000, 0x1000, CRC(0624ffd6) SHA1(8209ab633242f2c8952a5afe2b7cd399bab08f0a) )
	ROM_LOAD( "jrpacpe-04.ic1",     0x7000, 0x1000, CRC(d3a8448c) SHA1(f58aed6ebdb45ed38613b336a517b87745831e24) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "jrpacpe-09.ic92",    0x0000, 0x2000, CRC(2128d9b4) SHA1(b6f64423ae6ee3765050f7b85b4490b5eed95215) ) /* tiles bank 1 & 2 */
	ROM_LOAD( "jrpacpe-10.ic105",   0x2000, 0x2000, CRC(73477193) SHA1(f00a488958ea0438642d345693787bdf771219ad) ) /* sprites (bank 1) & 2 */

	ROM_REGION( 0x0120, "proms", 0 )
	ROM_LOAD_NIB_LOW ( "jrprom.9e", 0x0000, 0x0100, CRC(029d35c4) SHA1(d9aa2dc442e9ac36cf3c346b9fb1aa745eaf3cb8) ) /* color palette (low bits) */
	ROM_LOAD_NIB_HIGH( "jrprom.9f", 0x0000, 0x0100, CRC(eee34a79) SHA1(7561f8ccab2af85c111af6a02af6986eb67503e5) ) /* color palette (high bits) */
	ROM_LOAD( "jrprom.9p",          0x0020, 0x0100, CRC(9f6ea9d8) SHA1(62cf15513934d34641433c891a7f73bef82e2fb1) ) /* color lookup table */

	ROM_REGION( 0x0200, "namco", 0 )   
	ROM_LOAD( "jrprom.7p",          0x0000, 0x0100, CRC(a9cc86bf) SHA1(bbcec0570aeceb582ff8238a4bc8546a23430081) )
	ROM_LOAD( "jrprom.5s",          0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )
ROM_END



/*************************************
 *
 *  Driver initialization
 *
 *************************************/



void pengo_state::decode_penta(int end, int nodecend)
{
/*
    the values vary, but the translation mask is always laid out like this:

      0 1 2 3 4 5 6 7 8 9 a b c d e f
    0 A A B B A A B B C C D D C C D D
    1 A A B B A A B B C C D D C C D D
    2 E E F F E E F F G G H H G G H H
    3 E E F F E E F F G G H H G G H H
    4 A A B B A A B B C C D D C C D D
    5 A A B B A A B B C C D D C C D D
    6 E E F F E E F F G G H H G G H H
    7 E E F F E E F F G G H H G G H H
    8 H H G G H H G G F F E E F F E E
    9 H H G G H H G G F F E E F F E E
    a D D C C D D C C B B A A B B A A
    b D D C C D D C C B B A A B B A A
    c H H G G H H G G F F E E F F E E
    d H H G G H H G G F F E E F F E E
    e D D C C D D C C B B A A B B A A
    f D D C C D D C C B B A A B B A A

    (e.g. 0xc0 is XORed with H)
    therefore in the following tables we only keep track of A, B, C, D, E, F, G and H.
*/
	static const uint8_t data_xortable[2][8] =
	{
		{ 0xa0,0x82,0x28,0x0a,0x82,0xa0,0x0a,0x28 },    /* ...............0 */
		{ 0x88,0x0a,0x82,0x00,0x88,0x0a,0x82,0x00 }     /* ...............1 */
	};
	static const uint8_t opcode_xortable[8][8] =
	{
		{ 0x02,0x08,0x2a,0x20,0x20,0x2a,0x08,0x02 },    /* ...0...0...0.... */
		{ 0x88,0x88,0x00,0x00,0x88,0x88,0x00,0x00 },    /* ...0...0...1.... */
		{ 0x88,0x0a,0x82,0x00,0xa0,0x22,0xaa,0x28 },    /* ...0...1...0.... */
		{ 0x88,0x0a,0x82,0x00,0xa0,0x22,0xaa,0x28 },    /* ...0...1...1.... */
		{ 0x2a,0x08,0x2a,0x08,0x8a,0xa8,0x8a,0xa8 },    /* ...1...0...0.... */
		{ 0x2a,0x08,0x2a,0x08,0x8a,0xa8,0x8a,0xa8 },    /* ...1...0...1.... */
		{ 0x88,0x0a,0x82,0x00,0xa0,0x22,0xaa,0x28 },    /* ...1...1...0.... */
		{ 0x88,0x0a,0x82,0x00,0xa0,0x22,0xaa,0x28 }     /* ...1...1...1.... */
	};

	uint8_t *rom = memregion("maincpu")->base();

	for (int A = 0x0000;A < end;A++)
	{
		uint8_t src = rom[A];

		/* pick the translation table from bit 0 of the address */
		int i = A & 1;

		/* pick the offset in the table from bits 1, 3 and 5 of the source data */
		int j = ((src >> 1) & 1) + (((src >> 3) & 1) << 1) + (((src >> 5) & 1) << 2);
		/* the bottom half of the translation table is the mirror image of the top */
		if (src & 0x80) j = 7 - j;

		/* decode the ROM data */
		rom[A] = src ^ data_xortable[i][j];

		/* now decode the opcodes */
		/* pick the translation table from bits 4, 8 and 12 of the address */
		i = ((A >> 4) & 1) + (((A >> 8) & 1) << 1) + (((A >> 12) & 1) << 2);
		m_decrypted_opcodes[A] = src ^ opcode_xortable[i][j];
	}

	for (int A = end; A < nodecend; A++)
	{
		m_decrypted_opcodes[A] = rom[A];
	}
}

void pengo_state::init_penta()
{
	decode_penta(0x8000, 0x8000);
}


/*************************************
 *
 *  Game drivers
 *
 *************************************/

GAME( 1982, pengo,    0,        pengoe,   pengo,    pengo_state, empty_init,  ROT90, "Sega",                     "Pengo (set 1 rev C, encrypted)",      MACHINE_SUPPORTS_SAVE )
GAME( 1982, pengo2,   pengo,    pengoe,   pengo,    pengo_state, empty_init,  ROT90, "Sega",                     "Pengo (set 2, encrypted)",            MACHINE_SUPPORTS_SAVE )
GAME( 1982, pengo2u,  pengo,    pengou,   pengo,    pengo_state, empty_init,  ROT90, "Sega",                     "Pengo (set 2, rev A, not encrypted)", MACHINE_SUPPORTS_SAVE ) /* Sega game ID# 834-5092 PENGO REV.A */
GAME( 1982, pengo3u,  pengo,    pengou,   pengo,    pengo_state, empty_init,  ROT90, "Sega",                     "Pengo (set 3, not encrypted)",        MACHINE_SUPPORTS_SAVE ) /* Sega game ID# 834-5091 PENGO */
GAME( 1982, pengo4,   pengo,    pengoe,   pengo,    pengo_state, empty_init,  ROT90, "Sega",                     "Pengo (set 4, encrypted)",            MACHINE_SUPPORTS_SAVE ) /* Sega game ID# 834-5081 PENGO */
GAME( 1982, pengo5,   pengo,    pengoe,   pengo,    pengo_state, empty_init,  ROT90, "Sega",                     "Pengo (set 5, encrypted)",            MACHINE_SUPPORTS_SAVE )
GAME( 1982, pengob,   pengo,    pengo,    pengo,    pengo_state, init_penta,  ROT90, "bootleg",                  "Pengo (bootleg)",                     MACHINE_SUPPORTS_SAVE )
GAME( 1982, penta,    pengo,    pengo,    pengo,    pengo_state, init_penta,  ROT90, "bootleg (Grinbee Shouji)", "Penta",                               MACHINE_SUPPORTS_SAVE ) // Grinbee Shouji was a subsidiary of Orca
GAME( 1983, jrpacmbl, jrpacman, jrpacmbl, jrpacmbl, pengo_state, empty_init,  ROT90, "bootleg",                  "Jr. Pac-Man (Pengo hardware)",        MACHINE_SUPPORTS_SAVE )

// HBMAME start


// DSW1 needs to be here, coins/credits combinations to be found out
static INPUT_PORTS_START( pacpen )
	PORT_INCLUDE( pengo )
	PORT_MODIFY("DSW0")
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Bonus_Life ) )       PORT_DIPLOCATION("SW1:1,2")
	PORT_DIPSETTING(    0x00, "10000" )
	PORT_DIPSETTING(    0x01, "15000" )
	PORT_DIPSETTING(    0x02, "20000" )
	PORT_DIPSETTING(    0x03, DEF_STR( None ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Cabinet ) )          PORT_DIPLOCATION("SW1:3")
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x18, 0x10, DEF_STR( Lives ) )            PORT_DIPLOCATION("SW1:4,5")
	PORT_DIPSETTING(    0x18, "5" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_BIT(0xe0, IP_ACTIVE_LOW, IPT_UNUSED)               PORT_DIPLOCATION("SW1:6,7,8")
INPUT_PORTS_END



ROM_START( jrpacplus )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "jrpacplus-05.ic8",     0x0000, 0x1000, CRC(ea80c498) SHA1(f4a54d51fffa904443ab8e13f99328ed1b90ce4d) )
	ROM_LOAD( "jrpacplus-01.ic7",     0x1000, 0x1000, CRC(807299d4) SHA1(e6be61b0164c3d2d3f24138ca38e028aae1816e6) )
	ROM_LOAD( "jrpacplus-06.ic6",     0x2000, 0x1000, CRC(ac730069) SHA1(5e8842ce128bac43c8df80d20afc4b13fe932f28) )
	ROM_LOAD( "jrpacplus-02.ic5",     0x3000, 0x1000, CRC(c3ec10a8) SHA1(c7eaea9d41b107581dbd62f400ffda1288a67a5d) )
	ROM_LOAD( "jrpacplus-07.ic4",     0x4000, 0x1000, CRC(659b9956) SHA1(5576d4d95ced804e8abdd870662574bfdd6df18f) )
	ROM_LOAD( "jrpacplus-03.ic3",     0x5000, 0x1000, CRC(96f63003) SHA1(3884439c695d4bbe02389afb9a948eee9b19b35f) )
	ROM_LOAD( "jrpacplus-08.ic2",     0x6000, 0x1000, CRC(08d91441) SHA1(d0f2666e628a597e614dd46519f6e2f1d7f97e8e) )
	ROM_LOAD( "jrpacplus-04.ic1",     0x7000, 0x1000, CRC(7c361b65) SHA1(9377c683b036ccb15791ac5f01f7e310821e28d8) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "jrpacplus-09.ic92",    0x0000, 0x2000, CRC(f25d7aa2) SHA1(6b09cd9e7021e491b7871453ed370e580b50fb1a) )
	ROM_LOAD( "jrpacplus-10.ic105",   0x2000, 0x2000, CRC(5444032b) SHA1(07e140b3bb980c89155ad0a1e49399c75aa3898f) )

	ROM_REGION( 0x0120, "proms", 0 )
	ROM_LOAD_NIB_LOW ( "a290-27axv-bxhd.9e", 0x0000, 0x0100, CRC(029d35c4) SHA1(d9aa2dc442e9ac36cf3c346b9fb1aa745eaf3cb8) )
	ROM_LOAD_NIB_HIGH( "a290-27axv-cxhd.9f", 0x0000, 0x0100, CRC(eee34a79) SHA1(7561f8ccab2af85c111af6a02af6986eb67503e5) )
	ROM_LOAD( "jrpacplus.9p",         0x0020, 0x0100, CRC(deae51b1) SHA1(742210c473c4cb4489423ee60ab83f7a0110406d) )

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "a290-27axv-dxhd.7p",   0x0000, 0x0100, CRC(a9cc86bf) SHA1(bbcec0570aeceb582ff8238a4bc8546a23430081) )
	ROM_LOAD( "a290-27axv-exhd.5s",   0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )
ROM_END

ROM_START( pacpen )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "pacpen.u8",    0x0000, 0x1000, CRC(59959af1) SHA1(c528a7895338806977f2d9c76da91f67b0f78aed) )
	ROM_LOAD( "pacpen.u7",    0x1000, 0x1000, CRC(3ed59df1) SHA1(561725f5970954cccf938330fd9f48ad0cb1829b) )
	ROM_LOAD( "pacpen.u15",   0x2000, 0x1000, CRC(d0c8dcc8) SHA1(3a6829d12d7ef6f09d5e1afa5e0e4ef9d0605aa1) )
	ROM_LOAD( "pacpen.u14",   0x3000, 0x1000, CRC(39066ad9) SHA1(9789a111b94a3c5aabf910c72c146f71df59f316) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "pacpen.u92",   0x0000, 0x1000, CRC(2f0b5176) SHA1(a6a51d4545329999a5f42ae790bfb3a894429fee) )
	ROM_CONTINUE(             0x2000, 0x1000 )
	ROM_LOAD( "pacpen.105",   0x1000, 0x1000, CRC(6ba0078e) SHA1(a6e14aa373ef22fe732c335fb1ad6c48fec92530) )
	ROM_CONTINUE(             0x3000, 0x1000 )

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.ic78",    0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) )
	ROM_LOAD( "pr1634.ic88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) )

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.ic51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) )
	ROM_LOAD( "pr1636.ic70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )
ROM_END

ROM_START( pengopop )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "pengo.u8",     0x0000, 0x1000, CRC(3dfeb20e) SHA1(a387b72501da77bf38b58619d2099083a0463e1f) )
	ROM_LOAD( "pengo.u7",     0x1000, 0x1000, CRC(1db341bd) SHA1(d1c66bb9cf479e6960dbcd35c820097a81eaa555) )
	ROM_LOAD( "pengo.u15",    0x2000, 0x1000, CRC(7c2842d5) SHA1(a8a568da68babd0ccb9f2cee4182fc01c3138494) )
	ROM_LOAD( "pengo.u14",    0x3000, 0x1000, CRC(6e3c1f2f) SHA1(2ee821b0f6e0f3cfeae7f5ff25a6e9bd977efce0) )
	ROM_LOAD( "epr-5124.ic21",    0x4000, 0x1000, CRC(95f354ff) SHA1(fdebc68a6d87f8ecdf52a57a34ae5ae844a13510) )
	ROM_LOAD( "pengo.u20",    0x5000, 0x1000, CRC(0fdb04b8) SHA1(ed814d58318c1055e475ff678609d189727bf9b4) )
	ROM_LOAD( "epr-5126.ic32",    0x6000, 0x1000, CRC(e5920728) SHA1(0ac5ffdad7bdcb32e630b9582e1b1aaece5198c9) )
	ROM_LOAD( "pengopc.u31",  0x7000, 0x1000, CRC(1ede8569) SHA1(0d10a0896847a06185a91eb83c0ccb88c4307b33) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "epr-1640.ic92",    0x0000, 0x1000, CRC(d7eec6cd) SHA1(e542bcc28f292be9a0a29d949de726e0b55e654a) )
	ROM_CONTINUE(             0x2000, 0x1000 )
	ROM_LOAD( "epr-1695.ic105",   0x1000, 0x1000, CRC(5bfd26e9) SHA1(bdec535e486b43a8f5550334beff423eeace10b2) )
	ROM_CONTINUE(             0x3000, 0x1000 )

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.ic78",    0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) )
	ROM_LOAD( "pr1634.ic88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) )

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.ic51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) )
	ROM_LOAD( "pr1636.ic70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )
ROM_END

ROM_START( vecpengo )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "epr-1689c.ic8",    0x0000, 0x1000, CRC(f37066a8) SHA1(0930de17a763a527057f60783a92662b09554426) )
	ROM_LOAD( "epr-1690b.ic7",    0x1000, 0x1000, CRC(baf48143) SHA1(4c97529e61eeca5d94938b1dfbeac41bf8cbaf7d) )
	ROM_LOAD( "epr-1691b.ic15",   0x2000, 0x1000, CRC(adf0eba0) SHA1(c8949fbdbfe5023ee17a789ef60205e834a76c81) )
	ROM_LOAD( "epr-1692b.ic14",   0x3000, 0x1000, CRC(a086d60f) SHA1(7079769d14dfe3873ffe29623ba0a93413706c6d) )
	ROM_LOAD( "epr-1693b.ic21",   0x4000, 0x1000, CRC(b72084ec) SHA1(c0508951c2ad8dc31481be8b3bfee2063e3fb0d7) )
	ROM_LOAD( "epr-1694b.ic20",   0x5000, 0x1000, CRC(94194a89) SHA1(7b47aec61593efd758e2a031f72a854bb0ba8af1) )
	ROM_LOAD( "epr-5118b.ic32",   0x6000, 0x1000, CRC(af7b12c4) SHA1(207ed466546f40ca60a38031b83aef61446902e2) )
	ROM_LOAD( "epr-5119c.ic31",   0x7000, 0x1000, CRC(933950fe) SHA1(fec7236b3dee2ea6e39c68440a6d2d9e3f72675a) )

	ROM_REGION( 0x4000, "gfx1", 0 )
	ROM_LOAD( "vecp_ic92",    0x0000, 0x1000, CRC(57c5e53c) SHA1(4d1d4cdc352cb2fd14ebbd6678211093be73fb69) )
	ROM_CONTINUE(             0x2000, 0x1000 )
	ROM_LOAD( "vecp_ic105",   0x1000, 0x1000, CRC(b93588b0) SHA1(bbb779e538bdf7ebfcb0e12e11b57cabd5ddd29d) )
	ROM_CONTINUE(             0x3000, 0x1000 )

	ROM_REGION( 0x0420, "proms", 0 )
	ROM_LOAD( "pr1633.ic78",    0x0000, 0x0020, CRC(3a5844ec) SHA1(680eab0e1204c9b74adc11588461651b474021bb) )
	ROM_LOAD( "pr1634.ic88",    0x0020, 0x0400, CRC(766b139b) SHA1(3fcd66610fcaee814953a115bf5e04788923181f) )

	ROM_REGION( 0x0200, "namco", 0 )
	ROM_LOAD( "pr1635.ic51",    0x0000, 0x0100, CRC(c29dea27) SHA1(563c9770028fe39188e62630711589d6ed242a66) )
	ROM_LOAD( "pr1636.ic70",    0x0100, 0x0100, CRC(77245b66) SHA1(0c4d0bee858b97632411c440bea6948a74759746) )
ROM_END


GAME( 2021, jrpacplus, jrpacman, jrpacmbl, jrpacmbl, pengo_state, empty_init, ROT90, "Mr McScrewup", "Jr. Pac-Plus (Pengo hardware)", MACHINE_SUPPORTS_SAVE )
GAME( 2016, pacpen,    pengo,    pengou,   pacpen,   pengo_state, empty_init, ROT90, "Andrew Hannay", "Pacman on Pengo Hardware", MACHINE_SUPPORTS_SAVE )
GAME( 1997, pengopop,  pengo,    pengou,   pengo,    pengo_state, empty_init, ROT90, "Sega", "Pengo (Popcorn Music)", MACHINE_SUPPORTS_SAVE )
GAME( 2000, vecpengo,  pengo,    pengoe,   pengo,    pengo_state, empty_init, ROT90, "T-Bone", "Pengo (Vector sim)", MACHINE_SUPPORTS_SAVE )

