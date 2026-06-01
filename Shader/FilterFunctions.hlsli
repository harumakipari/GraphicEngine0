// RGB色空間の数値からHSV色空間の数値への変換関数
// rgb:RGB 色空間の数値
float3 RGB2HSV(float3 rgb)
{
    float3 hsv = 0;
    float Vmax = max(rgb.r, max(rgb.g, rgb.b)); // RGBの三つの値で最大のもの
    float Vmin = min(rgb.r, min(rgb.g, rgb.b)); // RGBの三つの値で最小のもの
    float delta = Vmax - Vmin; // 最大値と最小値の差

	// V（明度） 一番強い色をV値にする
    hsv.z = Vmax;

	// S（彩度） 最大値と最小値の差を正規化して求める
    hsv.y = (delta / Vmax) * step(0, Vmax);

	// H（色相） RGBのうち最大値と最小値の差から求める
    if (hsv.y > 0.0f)
    {
        if (rgb.r == Vmax)
        {
            hsv.x = 60 * ((rgb.g - rgb.b) / delta);
        }
        else if (rgb.g == Vmax)
        {
            hsv.x = 60 * ((rgb.b - rgb.r) / delta) + 120;
        }
        else
        {
            hsv.x = 60 * ((rgb.r - rgb.g) / delta) + 240;
        }

        if (hsv.x < 0)
        {
            hsv.x += 360;
        }
    }
    return hsv;
}

// HSV色空間の数値からRGB色空間の数値への変換関数
// hsv: HSV 色空間の数値
float3 HSV2RGB(float3 hsv)
{
    float3 rgb = 0;
    if (hsv.y == 0)
    {
        // S（彩度）が0と等しいならば無色もしくは灰色
        rgb.r = rgb.g = rgb.b = hsv.z;
    }
    else
    {
		// 色環のH（色相）の位置とS（彩度）、V（明度）からRGB値を算出する
        float Vmax = hsv.z;
        float Vmin = Vmax - (hsv.y * Vmax);

        hsv.x %= 360; //	0～360に変換
        float Huei = (int) (hsv.x / 60);
        float Huef = hsv.x / 60 - Huei;
        float p = Vmax * (1.0f - hsv.y);
        float q = Vmax * (1.0f - hsv.y * Huef);
        float t = Vmax * (1.0f - hsv.y * (1 - Huef));
        if (Huei == 0)
        {
            rgb.r = Vmax;
            rgb.g = t;
            rgb.b = p;
        }
        else if (Huei == 1)
        {
            rgb.r = q;
            rgb.g = Vmax;
            rgb.b = p;
        }
        else if (Huei == 2)
        {
            rgb.r = p;
            rgb.g = Vmax;
            rgb.b = t;
        }
        else if (Huei == 3)
        {
            rgb.r = p;
            rgb.g = q;
            rgb.b = Vmax;
        }
        else if (Huei == 4)
        {
            rgb.r = t;
            rgb.g = p;
            rgb.b = Vmax;
        }
        else if (Huei == 5)
        {
            rgb.r = Vmax;
            rgb.g = p;
            rgb.b = q;
        }
    }
    return rgb;
}

// 明るさとコントラストの調整関数
float3 BrightnessContrast(float3 fragmentColor, float brightness, float contrast)
{
	//明るさ - コントラスト効果
    //明るさ - コントラスト効果により、レンダリングされた画像の明るさとコントラストを変更できます。
    //明るさ: 画像の明るさ。範囲は -1 から 1 です（-1 は完全な黒、0 は変化なし、1 は完全な白）。
	//コントラスト：画像のコントラスト。範囲は-1から1（-1は完全な灰色、0は変化なし、1は最大コントラスト）。
    fragmentColor += brightness;
    if (contrast > 0.0)
    {
        fragmentColor = (fragmentColor - 0.5) / (1.0 - contrast) + 0.5;
    }
    else if (contrast < 0.0)
    {
        fragmentColor = (fragmentColor - 0.5) * (1.0 + contrast) + 0.5;
    }
    return fragmentColor;
}

// 色相と彩度の調整関数
float3 HueSaturation(float3 fragmentColor, float hue, float saturation)
{
	//色相 - 彩度効果
    //色相 - 彩度効果により、レンダリングされた画像の色相と彩度を変更できます。
    //色相: 画像の色相。範囲は -1 から 1 です（-1 は負方向の 180 度、0 は変更なし、1 は正方向の 180 度）。
	//彩度：画像の彩度。範囲は-1から1（-1は濃灰、0は変化なし、1は最大彩度）。
    float angle = hue * 3.14159265;
    float s = sin(angle), c = cos(angle);
    float3 weights = (float3(2.0 * c, -sqrt(3.0) * s - c, sqrt(3.0) * s - c) + 1.0) / 3.0;
    fragmentColor = float3(dot(fragmentColor, weights.xyz), dot(fragmentColor, weights.zxy), dot(fragmentColor, weights.yzx));
    float average = (fragmentColor.r + fragmentColor.g + fragmentColor.b) / 3.0;
    if (saturation > 0.0)
    {
        fragmentColor += (average - fragmentColor) * (1.0 - 1.0 / (1.001 - saturation));
    }
    else
    {
        fragmentColor += (average - fragmentColor) * (-saturation);
    }
    return fragmentColor;
}

// トーンマップ
float3 JodieReinhardToneMap(float3 c)
{
    float l = dot(c, float3(0.2126, 0.7152, 0.0722));
    float3 tc = c / (c + 1.0);

    return lerp(c / (l + 1.0), tc, tc);
}

float3 JodieReinhardGameToneMap(float3 c)
{
    return c / (c + 1.0);
}


// 定数バッファ側で渡す予定の補正値
// rgb.x -> 赤の強調カーブ
// rgb.y -> 緑の強調カーブ
// rgb.z -> 青の強調カーブ
float3 RGBColorMap(float3 sceneColor, float3 rgb)
{
    float3 result = sceneColor;

    // 赤
    result.r = pow(result.r, rgb.x); // rgb.x < 1 で赤を強調、>1 で弱め
    result.r = saturate(result.r * (1.0 + (1.0 - rgb.x))); // rgb.xに応じて掛け算で増幅/減衰

    // 緑
    result.g = pow(result.g, rgb.y); // rgb.y < 1 で緑を強調、>1で弱め
    result.g = saturate(result.g * (1.0 + (1.0 - rgb.y)));

    // 青
    result.b = pow(result.b, rgb.z); // rgb.z < 1 で青を強調、>1で弱め
    result.b = saturate(result.b * (1.0 + (1.0 - rgb.z)));

    return result;
}


float3 ToneCurve(float3 color, float value)
{
    if (value <= 0.0)
        return color;

    color = saturate(color);
    color = smoothstep(0.0, 1.0, color);
    color = lerp(color, color * color, value);

    return color;
}