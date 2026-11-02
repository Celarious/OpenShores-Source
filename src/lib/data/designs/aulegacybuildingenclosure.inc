// Included beside the existing building triangle/UV builder. Source: AuAtom13
// CreateMallTris 0x2842e0, CreateMallWall 0x285220, CreateMallBeam 0x2831f0,
// CreateMallRoof 0x283d80. The wall is a frame, not an invented opaque pane.
QVector3D EnclosurePoint(const EnclosureOptions& options, const QPointF& xy, double height)
{
	return options.project ? options.project(xy, height) : QVector3D(float(xy.x()), float(xy.y()), float(height));
}

void EnclosureQuad(Builder& out, const QVector3D& a, const QVector3D& b,
	const QVector3D& c, const QVector3D& d)
{
	const float repeat = 32.8083989501312f; // Original ten-metre texture repeat.
	const float u = (b - a).length() / repeat, v = (d - a).length() / repeat;
	out.TiledQuad(-1, 0, false, a, b, c, d, {0, 0}, {u, 0}, {u, v}, {0, v});
}

void EnclosureBeam(Builder& out, const QVector3D& a, const QVector3D& b,
	const QVector3D& upA, const QVector3D& upB, double radius, bool capA = true, bool capB = true)
{
	const QVector3D direction = b - a;
	if(direction.lengthSquared() < 1e-10f) return;
	QVector3D side = QVector3D::crossProduct(direction, upA + upB);
	if(side.lengthSquared() < 1e-10f) side = {1, 0, 0};
	side.normalize(); side *= float(radius);
	const QVector3D a0 = a - side, a1 = a + side, b0 = b - side, b1 = b + side;
	const QVector3D a2 = a1 - upA * float(2 * radius), a3 = a0 - upA * float(2 * radius);
	const QVector3D b2 = b1 - upB * float(2 * radius), b3 = b0 - upB * float(2 * radius);
	EnclosureQuad(out, a0, a1, b1, b0); EnclosureQuad(out, a2, a3, b3, b2);
	EnclosureQuad(out, a1, a2, b2, b1); EnclosureQuad(out, a3, a0, b0, b3);
	if(capA) EnclosureQuad(out, a0, a3, a2, a1);
	if(capB) EnclosureQuad(out, b1, b2, b3, b0);
}

void EnclosurePanel(Builder& out, const QVector3D& a, const QVector3D& b,
	const QVector3D& c, const QVector3D& d, double radius)
{
	QVector3D normal = QVector3D::crossProduct(b - a, d - a).normalized() * float(radius);
	if(normal.lengthSquared() < 1e-10f) return;
	EnclosureQuad(out, a + normal, b + normal, c + normal, d + normal);
	EnclosureQuad(out, b - normal, a - normal, d - normal, c - normal);
	EnclosureQuad(out, a - normal, b - normal, b + normal, a + normal);
	EnclosureQuad(out, d + normal, c + normal, c - normal, d - normal);
	EnclosureQuad(out, a - normal, a + normal, d + normal, d - normal);
	EnclosureQuad(out, b + normal, b - normal, c - normal, c + normal);
}

void EnclosureWallFrame(Builder& out, const EnclosureOptions& options,
	const QPointF& start, const QPointF& end, double lower, double upper, double radius, bool bottomBeam)
{
	const auto point = [&options](const QPointF& xy, double height) { return EnclosurePoint(options, xy, height); };
	const QVector3D a = point(start, lower), b = point(end, lower), c = point(end, upper), d = point(start, upper);
	const double length = (b - a).length();
	const int posts = std::max(1, 1 + int(length / (options.road ? 9.84251968503936 : 32.8083989501312)));
	if(posts < 2 || (d - a).length() < 3.28083989501312f || (c - b).length() < 3.28083989501312f)
	{ EnclosurePanel(out, a, b, c, d, radius); return; }
	const QVector3D upA = (point(start, 1) - point(start, 0)).normalized();
	const QVector3D upB = (point(end, 1) - point(end, 0)).normalized();
	EnclosureBeam(out, d, c, upA, upB, radius);
	if(bottomBeam) EnclosureBeam(out, a, b, upA, upB, radius);
	// 0x2854d5..0x285515: centers are inset one radius, with the remaining
	// span divided by posts-1, not another column added at each endpoint.
	const double step = std::max(0.0, length - 2 * radius) / (posts - 1);
	for(int post = 0; post < posts; ++post)
	{
		const QPointF xy = start + (end - start) * ((radius + step * post) / length);
		QVector3D along = b - a; along.normalize();
		const QVector3D offset = along * float(radius);
		EnclosureBeam(out, point(xy, lower) + offset, point(xy, upper) + offset, along, along, radius);
	}
}

bool EnclosureOnSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
	const QPointF v = b - a, q = p - a;
	const double length2 = QPointF::dotProduct(v, v);
	return length2 > 0 && std::abs(v.x() * q.y() - v.y() * q.x()) <= 1e-5 * std::sqrt(length2)
		&& QPointF::dotProduct(q, v) >= -1e-8 && QPointF::dotProduct(q, v) <= length2 + 1e-8;
}

QVector<QPair<double, double>> EnclosureExposed(const EnclosureOptions& options,
	const QPointF& a, const QPointF& b)
{
	QVector<double> cuts{0, 1}; const QLineF line(a, b);
	const QPointF direction = b - a;
	const double length2 = QPointF::dotProduct(direction, direction);
	if(length2 < 1e-10) return {};
	// AuMallBoundary samples a pressurized road at four-metre intervals.
	// Preserve those vertices before clipping the shared footprint boundary.
	if(options.road)
		for(double distance = 13.12335958005248; distance < std::sqrt(length2); distance += 13.12335958005248)
			cuts.append(distance / std::sqrt(length2));
	for(const auto& neighbor : options.neighbors)
		for(int index = 0; index < neighbor.boundary.size(); ++index)
		{
			const QPointF c = neighbor.boundary[index], d = neighbor.boundary[(index + 1) % neighbor.boundary.size()];
			QPointF intersection;
			if(line.intersects(QLineF(c, d), &intersection) == QLineF::BoundedIntersection)
				cuts.append(qBound(0.0, QPointF::dotProduct(intersection - a, direction) / length2, 1.0));
			if(EnclosureOnSegment(c, a, b)) cuts.append(qBound(0.0, QPointF::dotProduct(c - a, direction) / length2, 1.0));
		}
	std::sort(cuts.begin(), cuts.end());
	QVector<QPair<double, double>> result;
	// Source boundaries are clockwise: their left-hand normal points outward.
	const QPointF outward(-direction.y(), direction.x());
	const QPointF probe = outward * (.001 / std::sqrt(length2));
	for(int index = 1; index < cuts.size(); ++index)
	{
		if(cuts[index] - cuts[index - 1] < 1e-8) continue;
		const QPointF middle = a + direction * ((cuts[index] + cuts[index - 1]) * .5);
		bool covered = false;
		for(const auto& neighbor : options.neighbors)
		{
			if(neighbor.boundary.containsPoint(middle + probe, Qt::OddEvenFill)) { covered = true; break; }
			if(neighbor.owner >= options.site.owner) continue;
			for(int edge = 0; edge < neighbor.boundary.size(); ++edge)
				if(EnclosureOnSegment(middle, neighbor.boundary[edge], neighbor.boundary[(edge + 1) % neighbor.boundary.size()]))
				{ covered = true; break; }
			if(covered) break;
		}
		if(!covered) result.append(qMakePair(cuts[index - 1], cuts[index]));
	}
	return result;
}

void EnclosureGeometry(Builder& out, const EnclosureOptions& options)
{
	const auto& boundary = options.site.boundary;
	if(boundary.size() != 4) return;
	const double radius = options.undersea ? 1.5 : .5; // EnclosureColumnRadius 0xf0310.
	const QString frame = options.undersea ? QStringLiteral(":data/ConcreteHeavy.png") : QStringLiteral(":data/BronzeMetal.png");
	const auto up = [&options](const QPointF& xy) {
		return (EnclosurePoint(options, xy, 1) - EnclosurePoint(options, xy, 0)).normalized();
	};
	for(int edge = 0; edge < 4; ++edge)
	{
		// Road end segments are type1 in AuMallBoundary and remain open.
		if(options.road && (edge == 1 || edge == 3)) continue;
		const QPointF a = boundary[edge], b = boundary[(edge + 1) % 4];
		for(const auto& interval : EnclosureExposed(options, a, b))
		{
			const QPointF start = a + (b - a) * interval.first, end = a + (b - a) * interval.second;
			const double groundA = options.terrain ? options.terrain(start) : 0;
			const double groundB = options.terrain ? options.terrain(end) : 0;
			const double altitudeA = options.altitude ? options.altitude(start) : 0;
			const double altitudeB = options.altitude ? options.altitude(end) : 0;
			out.textureFile = QStringLiteral(":data/BrickLight.png");
			out.shininess = 0;
			// The foundation extends fifty feet below the developed ground.
			const QVector3D lowA = EnclosurePoint(options, start, groundA - 50), lowB = EnclosurePoint(options, end, groundB - 50);
			const QVector3D baseA = EnclosurePoint(options, start, 0), baseB = EnclosurePoint(options, end, 0);
			// AuMall::SetTerrainLevels marks land-enclosure foundation segments
			// below sea level as type1. Undersea enclosures retain those walls.
			if(options.undersea || groundA + altitudeA >= 0 || groundB + altitudeB >= 0)
				EnclosurePanel(out, lowA, lowB, baseB, baseA, radius);
			out.textureFile = frame;
			out.shininess = 500;
			EnclosureWallFrame(out, options, start, end, 0, 48, radius, false);
		}
	}
	out.textureFile = frame;
	out.shininess = 500;
	if(options.road)
	{
		// Original roads have two-segment roof members at four-metre samples.
		const QPointF start = (boundary[0] + boundary[3]) * .5, end = (boundary[1] + boundary[2]) * .5;
		const QPointF halfWidth = (boundary[3] - boundary[0]) * .5;
		const double length = QLineF(start, end).length();
		if(length < 1e-8) return;
		const int spans = std::max(1, int(std::ceil(length / 13.12335958005248)));
		for(int span = 0; span <= spans; ++span)
		{
			const QPointF center = start + (end - start) * std::min(1.0, span * 13.12335958005248 / length);
			const QPointF left = center - halfWidth, right = center + halfWidth;
			const QVector3D peak = EnclosurePoint(options, center, 47.5 + QLineF(left, right).length() * .25);
			EnclosureBeam(out, EnclosurePoint(options, left, 47.5), peak, up(left), up(center), radius);
			EnclosureBeam(out, peak, EnclosurePoint(options, right, 47.5), up(center), up(right), radius);
		}
		return;
	}
	const double roof = std::max(48.0, options.roofHeight);
	if(roof > 48)
		for(int edge = 0; edge < 4; ++edge)
		{
			const QPointF a = boundary[edge], b = boundary[(edge + 1) % 4];
			EnclosureWallFrame(out, options, a, b, 48, roof, radius, true);
		}
	const QPointF center = (boundary[0] + boundary[2]) * .5;
	const double width = QLineF(boundary[0], boundary[1]).length();
	for(int corner = 0; corner < 4; ++corner)
		for(int span = 0; span < 4; ++span)
		{
			const double a = span * .25, b = (span + 1) * .25;
			const QPointF first = boundary[corner] + (center - boundary[corner]) * a;
			const QPointF second = boundary[corner] + (center - boundary[corner]) * b;
			EnclosureBeam(out, EnclosurePoint(options, first, roof + std::sin(a * 1.5707963267948966) * width * .25),
				EnclosurePoint(options, second, roof + std::sin(b * 1.5707963267948966) * width * .25),
				up(first), up(second), radius, span == 0, false);
		}
}
