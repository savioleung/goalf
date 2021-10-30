#include<Siv3D.hpp>


P2Body setFloor(Vec2 pos, float size = 3, float rot = 0);
Array<P2Body> setGoal(Vec2 pos);
Vec2 getSceneVec2(Vec2 pos);
double twoPointDistance(Vec2 a, Vec2 b);
// 物理演算用のワールド
P2World world(50);

void Main()
{
#pragma region  変数
	//音
	Array<Audio> audioList = {
		//魔王魂
		Audio(U"Audio/hit.mp3") ,
		//“Music from https://www.zapsplat.com“
		Audio(U"Audio/makingStage.mp3"),
		Audio(U"Audio/Play.mp3"),
		Audio(U"Audio/Goal.mp3")
	};
	audioList[1].setLoop(true);
	audioList[2].setLoop(true);
	double volume = 0.1;
	//飛ぶ先を狙うゆび
	bool aiming = false;
	const Texture arrow(Emoji(U"👆"));

	Window::Resize(600, 600);
	Scene::SetBackground(ColorF(0.4, 0.7, 1.0));
	const Font font(15, Typeface::Bold);
	const Font font2(10, Typeface::Bold);
	// 物理演算のワールド更新に 60FPS の定数時間を使う場合は true, 実時間を使う場合 false
	constexpr bool useConstantDeltaTime = true;
	if (useConstantDeltaTime)
	{
		// フレームレート上限を 60 FPS に 
		Graphics::SetTargetFrameRateHz(60);
	}
	//GUI数値設定
	size_t inputMode = 0;
	double size = 3;
	double rot = 0;
	bool play = false;
	// 物理演算の精度
	constexpr int32 velocityIterations = 120;
	constexpr int32 positionIterations = 40;


	// 枠作る
	float x = 20, y = 30;
	x -= 1;
	y -= 1;
	Array<P2Body> frames;
	frames << world.createStaticLine(Scene::CenterF(), Line(-x, y, x, y), P2Material(0.0, 0.01, 0.0));//下
	frames << world.createStaticLine(Scene::CenterF(), Line(x, y, x, -y), P2Material(0.0, 0.01, 0.0));//右
	frames << world.createStaticLine(Scene::CenterF(), Line(x, -y, -x, -y), P2Material(0.0, 0.01, 0.0));//上
	frames << world.createStaticLine(Scene::CenterF(), Line(-x, -y, -x, y), P2Material(0.0, 0.01, 0.0));//左

	//初期設定座標
	Vec2 DefaultPos = Vec2(Scene::CenterF().x - 20, Scene::CenterF().y + 30);



	//壁ID  
	Array<P2BodyID> wallCollidedIDs;
	//地面ID
	Array<P2BodyID> floorCollidedIDs;
	//枠のID登録
	wallCollidedIDs << frames[1].id();
	wallCollidedIDs << frames[3].id();
	floorCollidedIDs << frames[0].id();
	floorCollidedIDs << frames[2].id();

	//足場設定
	Array<P2Body> floors;

	floors << setFloor(Vec2(10, 10), 3);
	floors << setFloor(Vec2(10, y), 3, -20_deg);
	floors << setFloor(Vec2(30, 20), 3);
	for (const auto& f : floors)
	{
		floorCollidedIDs << f.id();
	}
	Array<P2Body>Goal = setGoal(Vec2(30, 50));
	Vec2 enterPos, outPos, savePos;




	// キャンバスのサイズ
	constexpr Size canvasSize(600, 600);
	// ペンの太さ
	constexpr int32 thickness = 4;

	// 書き込み用の画像データを用意
	Image image(canvasSize, Color(0, 0));

	// 表示用のテクスチャ（内容を更新するので DynamicTexture）
	DynamicTexture texture(image);


	//ボール作り
	Vec2 startPos = Scene::CenterF(), spwanPos = Scene::CenterF();
	P2Body myBall = world.createDynamicCircle(startPos, 1, P2Material(10, 0.0, 0.2));
	P2BodyID myBallID = myBall.id();
	//ジャンプ回数
	int ballJump = 2;
	//ジャンプ可能性
	bool canJump = false, clicking = false, stopTime = false;
	int jumpNum = 0;

#pragma endregion
	TextEditState tes2;

	// 移動のオフセット
	Vec2 pos(40, 40);

	// Polygon 単純化時の基準距離（ピクセル）
	double maxDistance = 4.0;

	// 単純化した Polygon
	bool reset = false;
	//カメラ
	Camera2D camera(Vec2(Scene::CenterF().x + 10, Scene::CenterF().y), 10.0);
	while (System::Update())
	{
#pragma region 絵を描く
		//絵を描く

		if (!play && inputMode == 2)
		{

			if (MouseL.pressed())
			{
				// 書き込む線の始点は直前のフレームのマウスカーソル座標
				// （初回はタッチ操作時の座標のジャンプを防ぐため、現在のマウスカーソル座標にする）
				const Point from = MouseL.down() ? Cursor::Pos() : Cursor::PreviousPos();

				// 書き込む線の終点は現在のマウスカーソル座標
				const Point to = Cursor::Pos();

				// image に線を書き込む
				Line(from, to).overwrite(image, thickness, Palette::Gray);

				// 書き込み終わった image でテクスチャを更新
				texture.fill(image);
			}
			else if (MouseL.up())
			{
				// 画像の非透過部分から Polygon を作成（穴無し）
				if (const Polygon polygon = image.alphaToPolygon(1, false))
				{
					// Polygon を適切な位置に移動し、P2Body として追加
					const Polygon polygon2 = polygon.simplified(2.0)
						.moveBy((-canvasSize / 2).x + 10, (-canvasSize / 2).y).scale(1 / camera.getScale());
					floors << world.createStaticPolygon(camera.getCenter(), polygon2);
				}

				// 画像データをリセット
				image.fill(Color(0, 0));

				// テクスチャを更新
				texture.fill(image);
			}

		}
#pragma endregion


		//ワールドアップデート
		world.update(useConstantDeltaTime ? (1.0 / 60.0) : Scene::DeltaTime(), 12, 4);
		{

			// 描画用の Transformer2D
			const auto transformer = camera.createTransformer();
			//(+50,-20)
			P2Body testingLine = world.createKinematicLine(Vec2(DefaultPos.x + 50, DefaultPos.y - 20), Line(-size, 0, size, 0), P2Material(0.0, 0.01, 0.0)).setAngle(rot);

			//足場設置
			if (!play) {
				if (audioList[2].isPlaying()) {
					audioList[2].stop();
				}
				audioList[1].play();
#pragma region ステージ作り


				if (MouseL.pressed())
				{
					//ゲーム画面内のみ設置
					Vec2 PrintPos = Cursor::Pos() - DefaultPos;
					if (PrintPos.x < 39 && PrintPos.x>1 && PrintPos.y < -1 && PrintPos.y>-59) {

						switch (inputMode)
						{
						case 0://足場を設置
							//設置する足場の仮表示
							testingLine.setPos(DefaultPos.x + PrintPos.x, DefaultPos.y - -PrintPos.y);
							//右クイックで設置
							if (MouseR.down()) {
								floors << setFloor(Vec2(PrintPos.x, -PrintPos.y), size, rot);
								testingLine.setPos(DefaultPos.x + 50, DefaultPos.y - 20);
							}
							break;
						case 1:
							//ゴールの設置
							for (auto& g : Goal)
							{
								g.setPos(Vec2(DefaultPos.x + PrintPos.x, DefaultPos.y - -PrintPos.y));
							}
							break;
						case 3:
							//足場を画面外に移動（削除）
							for (auto& f : floors)
							{
								Vec2 floorV2 = f.getPos() - DefaultPos;
								double dis = twoPointDistance(floorV2, PrintPos);
								//マウスと足場の中心が距離１以下で削除
								if (dis <= 1) {
									f.setPos(Vec2(0, 0));
								}
							}
							break;
						case 4:
							//ボールのスタートポジション
							spwanPos = Vec2(DefaultPos.x + PrintPos.x, DefaultPos.y - -PrintPos.y);
							break;
						default:
							break;
						}

					}
				}

#pragma endregion

			}
			//遊べる時
			if (play) {
				if (audioList[1].isPlaying()) {
					audioList[1].stop();
				}
				audioList[2].play();
#pragma region 引っ張って飛ぶ

				if (MouseL.down() && !clicking)
				{
					aiming = true;
					enterPos = Cursor::Pos();
					clicking = true;
				}
				if (MouseL.up() && clicking)
				{
					aiming = false;
					//地面判定
					//発射時ボールが接触しているオブジェクト
					for (auto [pair, collision] : world.getCollisions())
					{
						//世界中の当たり判定の中でボールがある当たり判定
						if (pair.a == myBallID || pair.b == myBallID)
						{
							//壁に当たった場合
							if (wallCollidedIDs.includes(pair.a) || wallCollidedIDs.includes(pair.b)) {
								canJump = true;
							}
							else if (floorCollidedIDs.includes(pair.a) || floorCollidedIDs.includes(pair.b))
							{
								canJump = true;
								ballJump = 2;
							}

						}
					}
					outPos = Cursor::Pos();

					Vec2 ang;
					if (outPos != enterPos) {
						ang = (enterPos - outPos).normalized();
					}
					else
					{
						ang = Vec2(0, 0);
					}
					double dis = twoPointDistance(enterPos, outPos);

					//最小と最大の力を制限
					if (dis < 5) { dis = 5; }
					if (dis > 40) { dis = 40; }

					if (stopTime && ang == Vec2(0, 0)) {

						myBall.setVelocity(savePos);
						stopTime = false;
					}
					if (canJump) {
						if (ballJump > 0) {
							audioList[0].play();
							myBall.setVelocity(Vec2(0, 0));
							myBall.applyLinearImpulse(ang * dis * 35);
							ballJump--;
							jumpNum++;
							canJump = false;
						}
					}
					clicking = false;
				}

#pragma endregion


				//右クリックで遅くなる
				if (MouseR.pressed()) {
					myBall.setVelocity(myBall.getVelocity() * 0.98);

				}
				//常に判定取得処理
				for (auto [pair, collision] : world.getCollisions())
				{
					if (pair.a == myBallID || pair.b == myBallID)
					{
						//ゴールの底ついたらゴール判定
						if (pair.a == Goal[2].id() || pair.b == Goal[2].id()) {
							aiming = false;
							//ゴールの音声再生
							audioList[3].play();
						}
						else
						{
							audioList[3].stop();
							ClearPrint();
						}
						//壁タッチでもう一回だけジャンプ可能
						if (wallCollidedIDs.includes(pair.a) || wallCollidedIDs.includes(pair.b)) {
							canJump = true;
						}
					}
				}
			}
#pragma region 描画
			//出す足場テスト
			testingLine.draw(Palette::Black);
			P2Body ViewBall = world.createKinematicCircle(spwanPos, 1);
			//`描画
			// 枠を描画
			for (const auto& frame : frames)
			{
				frame.draw(Palette::Black);
			}
			//足場を描画
			for (const auto& f : floors)
			{
				if (!floorCollidedIDs.includes(f.id())) {
					floorCollidedIDs << f.id();
				}
				f.draw(Palette::Black);

			}

			//ゴールを描画
			for (const auto& g : Goal)
			{

				g.draw(Palette::Red);

			}

			if (play) {
				myBall.draw(Palette::Black);
			}
			else
			{
				ViewBall.draw(Palette::Orange);
			}
			if (aiming) {
				double ang = Math::Atan2(Cursor::Pos().y - enterPos.y, Cursor::Pos().x - enterPos.x);
				double dis = twoPointDistance(enterPos, Cursor::Pos());
				if (dis > 40) { dis = 40; }
				dis /= 2;
				//最小と最大の力を制限
				if (dis < 5) { dis = 5; }
				arrow.resized(5, dis).rotated(-90_deg + ang).drawAt(myBall.getPos());
			}

		}

#pragma endregion
#pragma region GUI

		SimpleGUI::RadioButtons(inputMode, { U"Floor", U"Goal",U"Pen Floor", U"Delete",U"Start Pos" }, Vec2(395, 10), 180, !play);
		SimpleGUI::Slider(U"Size:{:.1f}"_fmt(size), size, 1.0, 10.0, Vec2(395, 230), 80, 120, !play);
		SimpleGUI::Slider(U"Rot:{:.1f}"_fmt(rot), rot, 0.0_deg, 180.0_deg, Vec2(395, 280), 80, 120, !play);
		if (SimpleGUI::Button(U"Play", Vec2(395, 500)))
		{
			if (play) {

				myBall.setVelocity(Vec2(0, 1));
				clicking = false;
				canJump = false;
				play = false;
				aiming = false;
			}
			else
			{
				ClearPrint();
				inputMode = -1;
				myBall.setPos(spwanPos);
				myBall.setVelocity(Vec2(0, 1));
				clicking = false;
				canJump = false;
				jumpNum = 0;
				aiming = false;
				play = true;
			}
		}
		if (SimpleGUI::Slider(volume, Vec2(395, 550)))
		{
		}
		for (const auto& audio : audioList) {
			// 音量を設定 (0.0 - 1.0)
			audio.setVolume(volume);
		}

		texture.draw();
		font(U"Jump: {} times\n"_fmt(jumpNum)).draw(12, 20, ColorF(0.25));
		font2(U"音楽：魔王魂\nMusic from https://www.zapsplat.com").draw(395, 470, ColorF(0.25));

		// 単純化した Polygon の頂点数を表示
		ClearPrint();
	
#pragma endregion
		camera.draw(Palette::Orange);

	}
}




#pragma region 関数



double twoPointDistance(Vec2 a, Vec2 b) {
	double i, x, y;
	x = a.x - b.x;
	y = a.y - b.y;
	i = sqrt(x * x + y * y);
	return i;
}
//足場設定
P2Body setFloor(Vec2 pos, float size, float rot) {
	return world.createStaticLine(getSceneVec2(pos), Line(-size, 0, size, 0), P2Material(0.0, 0.01, 0.0)).setAngle(rot);
}
//ゴール設定
Array<P2Body> setGoal(Vec2 pos) {
	Array<P2Body> Goal;
	Goal << world.createStaticLine(getSceneVec2(pos), Line(3, 0, 1.2, 0), P2Material(0.0, 0.01, 0.0));
	Goal << world.createStaticLine(getSceneVec2(pos), Line(1.2, 0, 1.2, 2.4), P2Material(0.0, 0.01, 0.0));
	Goal << world.createStaticLine(getSceneVec2(pos), Line(1.2, 2.4, -1.2, 2.4), P2Material(0.0, 0.01, 0.0));
	Goal << world.createStaticLine(getSceneVec2(pos), Line(1.2, 2.5, -1.2, 2.5), P2Material(0.0, 0.01, 0.0));
	Goal << world.createStaticLine(getSceneVec2(pos), Line(-1.2, 2.4, -1.2, 0), P2Material(0.0, 0.01, 0.0));
	Goal << world.createStaticLine(getSceneVec2(pos), Line(-1.2, 0, -3, 0), P2Material(0.0, 0.01, 0.0));
	return Goal;
}
Vec2 getSceneVec2(Vec2 pos) {
	return Vec2(Scene::CenterF().x - 20 + pos.x, Scene::CenterF().y + 30 - pos.y);
}

#pragma endregion

