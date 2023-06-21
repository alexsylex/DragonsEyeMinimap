import gfx.io.GameDelegate;
import Map.MapMenu;

// Width: 845.7, Height: 559.3, x: 240, y: 130
class Map.LocalMap extends MovieClip
{
	var BottomBar: MovieClip;
	var ClearedDescription: TextField;
	var ClearedText: TextField;
	var IconDisplay: MapMenu;
	var LocalMapHolder_mc: MovieClip;
	var LocationDescription: TextField;
	var LocationTextClip: MovieClip;
	var MapImageLoader: MovieClipLoader;
	var TextureHolder: MovieClip;
	var _TextureHeight: Number;
	var _TextureWidth: Number;
	var bUpdated;

	var bShown: Boolean;

	var testTf:TextField;

	function LocalMap()
	{
		super();
		IconDisplay = new MapMenu(this);

		testTf = _root.map.TestTf;
		testTf.text = "Hello from local map";

		MapImageLoader = new MovieClipLoader();
		MapImageLoader.addListener(this);
		_TextureWidth = 800;
		_TextureHeight = 450;
		LocationDescription = LocationTextClip.LocationText;
		LocationDescription.noTranslate = true;
		LocationTextClip.swapDepths(3);
		ClearedDescription = ClearedText;
		ClearedDescription.noTranslate = true;
		TextureHolder = LocalMapHolder_mc;
	}

	function get TextureWidth(): Number
	{
		return _TextureWidth;
	}

	function get TextureHeight(): Number
	{
		return _TextureHeight;
	}

	function onLoadInit(TargetClip: MovieClip): Void
	{
		TargetClip._width = _TextureWidth;
		TargetClip._height = _TextureHeight;
	}

	function InitMap(): Void
	{
		if (!bUpdated) {
			MapImageLoader.loadClip("img://Local_Map", TextureHolder);
			bUpdated = true;
		}
		var textureTopLeft: Object = {x: _x, y: _y};
		var textureBottomRight: Object = {x: _x + _TextureWidth, y: _y + _TextureHeight};
		_parent.localToGlobal(textureTopLeft);
		_parent.localToGlobal(textureBottomRight);
		GameDelegate.call("SetLocalMapExtents", [textureTopLeft.x, textureTopLeft.y, textureBottomRight.x, textureBottomRight.y]);
	}

	function Show(abShow: Boolean): Void
	{
		if (abShow != bShown)
		{
			_parent.gotoAndPlay(abShow ? "fadeIn" : "fadeOut");
			BottomBar.RightButton.visible = !abShow;
			BottomBar.LocalMapButton.label = abShow ? "$World Map" : "$Local Map";
			bShown = abShow;
		}
	}

	function SetBottomBar(aBottomBar: MovieClip): Void
	{
		BottomBar = aBottomBar;
	}

	function SetTitle(aName: String, aCleared: String): Void
	{
		LocationDescription.text = aName == undefined ? "" : aName;
		ClearedDescription.text = aCleared == undefined ? "" : "(" + aCleared + ")";
	}

}