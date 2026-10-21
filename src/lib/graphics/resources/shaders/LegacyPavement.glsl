// Original 2016 AuGL13.dll TerrainLitNoA.f.glsl pavement block.
// Shared by the legacy globe and ringworld terrain fragment shaders.
	// Pavement
	{
		int paveTileIndex = min(int(round(vertPaveType * 18)), 17);				// Tile index 0..17
		paveHard = paveTileIndex <= 10;											// 2016 hard pavement is 0..10, see DaWorldLL::PavementHidesSnow.
		vec4 paveColor;
		if(paveTileIndex == 0)
			paveColor = texture(texPavement[0], terCoord3);
		else if(paveTileIndex == 1)
			paveColor = texture(texPavement[1], terCoord3);
		else if(paveTileIndex == 2)
			paveColor = texture(texPavement[2], terCoord3);
		else if(paveTileIndex == 3)
			paveColor = texture(texPavement[3], terCoord3);
		else if(paveTileIndex == 4)
			paveColor = texture(texPavement[4], terCoord3);
		else if(paveTileIndex == 5)
			paveColor = texture(texPavement[5], terCoord3);
		else if(paveTileIndex == 6)
			paveColor = texture(texPavement[6], terCoord3);
		else if(paveTileIndex == 7)
			paveColor = texture(texPavement[7], terCoord3);
		else if(paveTileIndex == 8)
			paveColor = texture(texPavement[8], terCoord3);
		else if(paveTileIndex == 9)
			paveColor = texture(texPavement[9], terCoord3);
		else if(paveTileIndex == 10)
			paveColor = texture(texPavement[10], terCoord3);
		else if(paveTileIndex == 11)
			paveColor = texture(texPavement[11], terCoord3);
		else if(paveTileIndex == 12)
			paveColor = texture(texPavement[12], terCoord3);
		else if(paveTileIndex == 13)
			paveColor = texture(texPavement[13], terCoord3);
		else if(paveTileIndex == 14)
			paveColor = texture(texPavement[14], terCoord3);
		else if(paveTileIndex == 15)
			paveColor = texture(texPavement[15], terCoord3);
		else if(paveTileIndex == 16)
			paveColor = texture(texPavement[16], terCoord3);
		else
			paveColor = texture(texPavement[17], terCoord3);

		// Combine pavement with terrain color.
		fragColor = mix(fragColor, paveColor, paveCover*paveColor.a);
	}

