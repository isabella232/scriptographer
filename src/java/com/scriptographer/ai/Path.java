/*
 * Scriptographer
 *
 * This file is part of Scriptographer, a Plugin for Adobe Illustrator.
 *
 * Copyright (c) 2002-2008 Juerg Lehni, http://www.scratchdisk.com.
 * All rights reserved.
 *
 * Please visit http://scriptographer.com/ for updates and contact.
 *
 * -- GPL LICENSE NOTICE --
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * -- GPL LICENSE NOTICE --
 *
 * File created on 03.12.2004.
 *
 * $Id$
 */

package com.scriptographer.ai;

import java.awt.Shape;
import java.awt.geom.GeneralPath;
import java.awt.geom.PathIterator;
import java.util.ArrayList;

import com.scratchdisk.list.ExtendedList;
import com.scratchdisk.list.List;
import com.scratchdisk.list.Lists;
import com.scriptographer.CommitManager;

/**
 * @author lehni
 */
public class Path extends PathItem {

	private SegmentList segments = null;
	private CurveList curves = null;

	/**
	 * Wraps an AIArtHandle in a Path object
	 */
	protected Path(int handle) {
		super(handle);
	}

	/**
	 * Creates a path object of the given type. Used by CompoundPath
	 */
	protected Path(short type) {
		super(type);
	}

	/**
	 * Creates a path item
	 */
	public Path() {
		super(TYPE_PATH);
	}

	public Path(List segments) {
		this();
		setSegments(segments);
	}

	public Path(Object[] segments) {
		this(Lists.asList(segments));
	}
	
	public Path(Shape shape) {
		this();
		append(shape);
	}

	/**
	 * Removes the path item from the document.
	 */
	public boolean remove() {
        boolean ret = super.remove();
        // Dereference from path if they're used somewhere else!
        if (segments != null)
            segments.path = null;
        return ret;
    }
    
	public Object clone() {
		CommitManager.commit(this);
		return super.clone();
	}

	public SegmentList getSegments() {
		if (segments == null)
			segments = new SegmentList(this);
		else
			segments.update();
		return segments;
	}

	public void setSegments(List list) {
		SegmentList segments = getSegments();
		// TODO: Implement SegmentList.setAll so clear is not necessary and
		// nativeCommit is used instead of nativeInsert removeRange would still
		// be needed in cases the new list is smaller than the old one...
		segments.removeAll();
		segments.addAll(list);
	}

	public void setSegments(Segment[] segments) {
		setSegments(Lists.asList(segments));
	}

	public CurveList getCurves() {
		if (curves == null)
			curves = new CurveList(this, getSegments());
		return curves;
	}

	public native boolean isClosed();
	
	private native void nativeSetClosed(boolean closed);
	
	public void setClosed(boolean closed) {
		// Amount of curves may change when closed is modified
		nativeSetClosed(closed);
		if (curves != null)
			curves.updateSize();
	}
	
	public native boolean isGuide();
	
	public native void setGuide(boolean guide);
	
	public native TabletValue[] getTabletData();
	
	public native void setTabletData(TabletValue[] data);
	
	public void setTabletData(float[][] data) {
		// Convert to a TabletValue[] data array:
		ArrayList values = new ArrayList();
		for (int i = 0; i < data.length; i++) {
			float[] pair = data[i];
			if (pair != null && pair.length >= 2)
				values.add(new TabletValue(pair[0], pair[1]));
		}
		TabletValue[] tabletData = new TabletValue[values.size()];
		values.toArray(tabletData);
		setTabletData(tabletData);
	}

	public native float getLength(float flatness);

	public float getLength() {
		return getLength(Curve.FLATNESS);
	}

	public native float getArea();

	private native void nativeReverse();

	/**
	 * Reverses the segments of a path. This is only important for sub paths of
	 * compound paths, since the winding order controls the insideness of the
	 * compound path.
	 */
	public void reverse() {
		// First save all changes:
		CommitManager.commit(this);
		// Reverse underlying AI structures:
		nativeReverse();
		// Increase version as all segments have changed
		this.version++;
	}
	
	private void updateSize(int size) {
		// increase version as all segments have changed
		this.version++;
		if (segments != null)
			segments.updateSize(size);
		
	}

	private native int nativePointsToCurves(float tolerance, float threshold,
			int cornerRadius, float scale);

	public void pointsToCurves(float tolerance, float threshold,
			int cornerRadius, float scale) {
		updateSize(nativePointsToCurves(tolerance, threshold, cornerRadius,
				scale));
	}

	public void pointsToCurves(float tolerance, float threshold,
			int cornerRadius) {
		pointsToCurves(tolerance, threshold, cornerRadius, 1f);
	}

	public void pointsToCurves(float tolerance, float threshold) {
		pointsToCurves(tolerance, threshold, 1, 1f);
	}

	public void pointsToCurves(float tolerance) {
		pointsToCurves(tolerance, 1f, 1, 1f);
	}

	public void pointsToCurves() {
		pointsToCurves(2.5f, 1f, 1, 1f);
	}

	private native int nativeCurvesToPoints(float maxPointDistance,
			float flatness);

	public void curvesToPoints(float maxPointDistance, float flatness) {
		int size = nativeCurvesToPoints(maxPointDistance, flatness);
		updateSize(size);
	}

	public void curvesToPoints(float maxPointDistance) {
		curvesToPoints(maxPointDistance, Curve.FLATNESS);
	}

	public void curvesToPoints() {
		curvesToPoints(1000f, Curve.FLATNESS);
	}

	private native void nativeReduceSegments(float flatness);

	public void reduceSegments(float flatness) {
		nativeReduceSegments(flatness);
		updateSize(-1);
	}

	public void reduceSegments() {
		reduceSegments(Curve.FLATNESS);
	}
	
	public Path split(float position) {
		int index = (int) Math.floor(position);
		float parameter = position - index;
		return this.split(index, parameter);
	}

	public Path split(int index, float parameter) {
		SegmentList segments = getSegments();
		ExtendedList newSegments = null;

		if (parameter < 0.0f) parameter = 0.0f;
		else if (parameter >= 1.0f) {
			// t = 1 is the same as t = 0 and index ++
			index++;
			parameter = 0.0f;
		}
		if (index >= 0 && index < segments.size - 1) {
			if (parameter == 0.0) { // spezial case
				if (index > 0) {
					// split at index
					newSegments = segments.getSubList(index, segments.size);
					segments.remove(index + 1, segments.size);
				}
			} else {
				// divide the segment at index at parameter
				Segment segment = (Segment) segments.get(index);
				if (segment != null) {
					segment.divide(parameter);
					// create the new path with the segments to the right of t
					newSegments = segments.getSubList(index + 1, segments.size);
					// and delete these segments from the current path, not
					// including the divided point
					segments.remove(index + 2, segments.size);
				}
			}
		}
		if (newSegments != null)
			return new Path(newSegments);
		else
			return null;
	}

	public Path split(int index) {
		return split(index, 0f);
	}

	public HitTest hitTest(Point point, float epsilon) {
		CurveList curves = getCurves();
		int length = curves.size();
		
		for (int i = 0; i < length; i++) {
			Curve curve = (Curve) curves.get(i);
			float t = curve.hitTest(point, epsilon);
			if (t >= 0)
				return new HitTest(curve, t);
		}
		return null;
	}

	public HitTest hitTest(Point point) {
		return hitTest(point, Curve.EPSILON);
	}

	public HitTest getPositionWithLength(float length, float flatness) {
		CurveList curves = getCurves();
		float currentLength = 0;
		for (int i = 0; i < curves.size; i++) {
			float startLength = currentLength;
			Curve curve = (Curve) curves.get(i);
			currentLength += curve.getLength(flatness);
			if (currentLength >= length) {
				// found the segment within which the length lies
				float t = curve.getParameterWithLength(length - startLength,
						flatness);
				return new HitTest(curve, t);
			}
		}
		// it may be that through unpreciseness of getLength, that the end of
		// the curves was missed:
		if (length <= getLength(flatness)) {
			Curve curve = (Curve) curves.get(curves.size - 1);
			return new HitTest(HitTest.HIT_ANCHOR, curve, 1, curve.getPoint2());
		} else {
			return null;
		}
	}

	public HitTest getPositionWithLength(float length) {
		return getPositionWithLength(length, Curve.FLATNESS);
	}
	
	/*
	 *  PostScript-like interface: moveTo, lineTo, curveTo, arcTo
	 */

	public void moveTo(float x, float y) {
		getSegments().moveTo(x, y);
	}
	
	public void lineTo(float x, float y) {
		getSegments().lineTo(x, y);
	}
	
	public void curveTo(float c1x, float c1y, float c2x, float c2y, float x,
			float y) {
		getSegments().curveTo(c1x, c1y, c2x, c2y, x, y);
	}
	
	public void quadTo(float cx, float cy, float x, float y) {
		getSegments().quadTo(cx, cy, x, y);
	}
	
	public void arcTo(float centerX, float centerY, float endX, float endY,
			int ccw) {
		getSegments().arcTo(centerX, centerY, endX, endY, ccw);
	}

	public void closePath() {
		setClosed(true);
	}

	/*
	 * Convert to and from Java2D (java.awt.geom)
	 */

	/**
	 * Appends the segments of a PathIterator to this Path. Optionally, the
	 * initial {@link PathIterator#SEG_MOVETO}segment of the appended path is
	 * changed into a {@link PathIterator#SEG_LINETO}segment.
	 * 
	 * @param iter the PathIterator specifying which segments shall be appended.
	 * @param connect <code>true</code> for substituting the initial
	 *        {@link PathIterator#SEG_MOVETO}segment by a {@link
	 *        PathIterator#SEG_LINETO}, or <code>false</code> for not
	 *        performing any substitution. If this GeneralPath is currently
	 *        empty, <code>connect</code> is assumed to be <code>false</code>,
	 *        thus leaving the initial {@link PathIterator#SEG_MOVETO}unchanged.
	 */
	public void append(PathIterator iter, boolean connect) {
		float[] f = new float[6];
		SegmentList segments = getSegments();
		int size = segments.size();
		boolean open = true;
		while (!iter.isDone() && open) {
			switch (iter.currentSegment(f)) {
				case PathIterator.SEG_MOVETO:
					if (!connect || (size == 0)) {
						moveTo(f[0], f[1]);
						break;
					}
					if (size >= 1) {
						Point pt = ((Segment) segments.getLast()).point;
						if (pt.x == f[0] && pt.y == f[1])
							break;
					}
					// Fall through to lineto for connect!
				case PathIterator.SEG_LINETO:
					segments.lineTo(f[0], f[1]);
					break;
				case PathIterator.SEG_QUADTO:
					quadTo(f[0], f[1], f[2], f[3]);
					break;
				case PathIterator.SEG_CUBICTO:
					segments.curveTo(f[0], f[1], f[2], f[3], f[4], f[5]);
					break;
				case PathIterator.SEG_CLOSE:
					setClosed(true);
					open = false;
					break;
			}

			// connect = false;
			iter.next();
		}
	}

	public GeneralPath toShape() {
		GeneralPath path = new GeneralPath();
		SegmentList segments = getSegments();
		Segment first = (Segment) segments.getFirst();
		path.moveTo(first.point.x, first.point.y);
		Segment seg = first;
		for (int i = 1; i < segments.size; i++) {
			Segment next = (Segment) segments.get(i);
			path.curveTo(seg.point.x + seg.handleOut.x, seg.point.y + seg.handleOut.y,
					next.point.x + next.handleIn.x, next.point.y + next.handleIn.y,
					next.point.x, next.point.y);
			seg = next;
		}
		if (isClosed()) {
			path.curveTo(seg.point.x + seg.handleOut.x, seg.point.y + seg.handleOut.y,
					first.point.x + first.handleIn.x, first.point.y + first.handleIn.y,
					first.point.x, first.point.y);
			path.closePath();
		}
		Boolean evenOdd = getStyle().getEvenOdd();
		path.setWindingRule(evenOdd != null && evenOdd.booleanValue()
				? GeneralPath.WIND_EVEN_ODD
				: GeneralPath.WIND_NON_ZERO);
		return path;
	}
}