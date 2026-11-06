// Original 2016 AuGL13.dll TerrainLitNoA.f.glsl pavement block.
// Shared by the legacy globe and ringworld terrain fragment shaders.
	// Pavement
	{
		int paveTileIndex = min(int(round(vertPaveType * 18)), 17);				// Tile index 0..17
		bool paintedRoad = abs(vertRoadPaint.w) > 0.0
			&& abs(vertRoadPaint.x) <= abs(vertRoadPaint.w)
			&& vertRoadPaint.y >= 0.0 && vertRoadPaint.y <= abs(vertRoadPaint.z)
			&& (paveTileIndex == 3 || paveTileIndex == 4);
		if(paintedRoad) paveTileIndex = vertRoadPaint.w < 0.0 ? 4 : 3;
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

		if(paintedRoad)
		{
			const float RoadMarkerWidth = .25;
			const float SlabMarkerWidth = 1.;
			if(vertRoadPaint.w < 0.0) paveColor.rgb *= .35;
			// Six-inch yellow paint, ten-foot dashes and twenty-foot gaps.
			// Streets use their centreline; slabs use an inset rectangle.
			float lineDistance = abs(vertRoadPaint.x);
			float along = vertRoadPaint.y;
			float dashPeriod = 30.0;
			bool paintFits = true;
			if(vertRoadPaint.z < 0.0)
			{
				vec2 halfSize = vec2(abs(vertRoadPaint.w), -vertRoadPaint.z * .5) - vec2(10.0);
				vec2 point = vec2(vertRoadPaint.x, vertRoadPaint.y + vertRoadPaint.z * .5);
				vec2 edge = abs(point) - halfSize;
				lineDistance = abs(max(edge.x, edge.y));
				// Walk the four edges from the bottom-left corner, so dash
				// phase remains continuous as it turns each corner.
				if(edge.x > edge.y)
					along = point.x >= 0.0 ? 2.0 * halfSize.x + point.y + halfSize.y
						: 4.0 * halfSize.x + 3.0 * halfSize.y - point.y;
				else
					along = point.y >= 0.0 ? 3.0 * halfSize.x + 2.0 * halfSize.y - point.x
						: point.x + halfSize.x;
				paintFits = all(greaterThan(halfSize, vec2(.25)));
				// Fit whole dash/gap repetitions around the closed border.
				float perimeter = max(4.0 * (halfSize.x + halfSize.y), 1.0);
				dashPeriod = perimeter / max(1.0, round(perimeter / 30.0));
			}
			float sideSoft = max(fwidth(lineDistance), .001);
			float alongSoft = max(fwidth(along), .001);
			float MarkerWidth = vertRoadPaint.z < 0.0 ? SlabMarkerWidth : RoadMarkerWidth;
			float stripe = paintFits ? 1.0 - smoothstep(MarkerWidth - sideSoft, MarkerWidth + sideSoft, lineDistance) : 0.0;
			float dashDistance = abs(mod(along + dashPeriod / 3.0, dashPeriod) - dashPeriod * .5) - dashPeriod / 6.0;
			float dash = 1.0 - smoothstep(-alongSoft, alongSoft, dashDistance);
			paveColor.rgb = mix(paveColor.rgb, vec3(.95, .72, .08), stripe * dash);
		}
		// Combine pavement with terrain color.
		fragColor = mix(fragColor, paveColor, paveCover*paveColor.a);
	}

