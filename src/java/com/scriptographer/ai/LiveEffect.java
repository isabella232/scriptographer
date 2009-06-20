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
 * File created on 18.02.2005.
 * 
 * $Id:LiveEffect.java 402 2007-08-22 23:24:49Z lehni $
 */

package com.scriptographer.ai;

import com.scriptographer.ScriptographerEngine; 
import com.scriptographer.ScriptographerException;
import com.scratchdisk.script.Callable;
import com.scratchdisk.util.ConversionUtils;
import com.scratchdisk.util.IntMap;
import com.scriptographer.ui.MenuItem;

import java.util.Map;
import java.util.ArrayList;

/**
 * Wrapper for Illustrator's LiveEffects. Unfortunately, Illustrator is not
 * able to remove once created effects again until the next restart. They can be
 * removed from the menu but not from memory. So In order to recycle effects
 * with the same settings, e.g. during development, where the code often changes
 * but the initial settings maybe not, keep track of all existing effects and
 * match against those first before creating a new one. Also, when
 * Scriptographer is (re)loaded, the list of existing effects needs to be walked
 * through and added to the list of unusedEffects. This is done by calling
 * getUnusedEffects.
 * 
 * @author lehni
 * 
 * @jshide
 */

/*
Re: sdk: Illustrator AILiveEffect questions
Datum: 23. Februar 2005 18:53:13 GMT+01:00

In general, live effects should run on all the art it is given. In the first example of running a post effect on a
path, the input art is split into two objects: one path that contains just the stroke attributes and another path that
contains just the fill color. Input art is typically split up this way when effects are involved.

If the effect is dragged before any of the fill/stroke layers in the appearance palette, then the input path the effect
will see will just be one path, since it has not been split up yet. However, the input path will contain no paint,
since it has not gone through the fill/stroke layers yet.

In the example of running a post effect on a group with two paths, the input art will be split up again in order to go
through the fill/stroke layers and the "Contents" layer. Thus, you will see three copies of the group: one that may
just be filled, one that may just be stroked, and one that contains the original group unchanged.

Because of this redundancy, it is more optimal to register effects as pre effects, if the effect does not care about
the paint on the input path. For example, the Roughen effect in Illustrator is registered as a pre effect since it
merely roughens the geometry, regardless of the paint. On the other hand, drop shadow is registered as a post effect,
since its results depend on the paint applied to the input objects.

While there is redundancy, effects really cannot make any assumptions about the input art they are given and should
thus attempt to operate on all of the input art. At the end of executing an entire appearance, Illustrator will attempt
to "clean up" and remove any unnecessary nested groups and unpainted paths.

When creating output art for the go message, the output art must be a child of the same parent as the input art. It
also must be the only child of this parent, so if you create a copy of the input art, work on it and attempt to return
the copy as the output art, you must make sure to dispose the original input art first. It is not legal to create an
item in an arbitrary place and return that as the output art.

Effects are limited in the kinds of attributes that they can attach to the output art. Effects must restrict themselves
to using "simple" attributes, such as:
- 1 fill and 1 stroke (AIPathStyle)
- transparency options (AIBlendStyle)
It is actually not necessary to use the AIArtStyle suite when generating output art, and effects should try to avoid
it. Effects also should avoid putting properties on output art that will generate more styled art (ie. nested styled
art is not allowed).

I suggest playing around with the TwirlFilterProject in Illustrator and expanding the appearance to get a better
picture of the live effect architecture.

Hope that helps,
-Frank
*/
public class LiveEffect extends NativeObject {
	// AIStyleFilterPreferredInputArtType
	public static final int
		INPUT_DYNAMIC	 		= 0,
		INPUT_GROUP 			= 1 << (Item.TYPE_GROUP - 1),
		INPUT_PATH 				= 1 << (Item.TYPE_PATH - 1),
		INPUT_COMPOUNDPATH 		= 1 << (Item.TYPE_COMPOUNDPATH - 1),

		INPUT_PLACED 			= 1 << (Item.TYPE_PLACED - 1),
		INPUT_MYSTERYPATH 		= 1 << (Item.TYPE_MYSTERYPATH - 1),
		INPUT_RASTER 			= 1 << (Item.TYPE_RASTER - 1),

		// If INPUT_PLUGIN is not specified, the filter will receive the result group of a plugin
		// group instead of the plugin group itself
		INPUT_PLUGIN			= 1 << (Item.TYPE_PLUGIN - 1),
		INPUT_MESH 				= 1 << (Item.TYPE_MESH - 1),

		INPUT_TEXTFRAME 		= 1 << (Item.TYPE_TEXTFRAME - 1),

		INPUT_SYMBOL 			= 1 << (Item.TYPE_SYMBOL - 1),

		INPUT_FOREIGN			= 1 << (Item.TYPE_FOREIGN - 1),
		INPUT_LEGACYTEXT		= 1 << (Item.TYPE_LEGACYTEXT - 1),

		// Indicates that the effect can operate on any input art. */
		INPUT_ANY 				= 0xfff,
		// Indicates that the effect can operate on any input art other than plugin groups which
		// are replaced by their result art.
		INPUT_ANY_BUT_PLUGIN	= INPUT_ANY & ~INPUT_PLUGIN,

		// Special values that don't correspond to regular art types should be in the high half word

		// Wants strokes to be converted to outlines before being filtered (not currently implemented)
		INPUT_OUTLINED_STROKE	= 0x10000,
		// Doesn't want to take objects that are clipping paths or clipping text (because it destroys them,
		// e.g. by rasterizing, or by splitting a single path into multiple non-intersecting paths,
		// or by turning it into a plugin group, like the brush filter).
		// This flag is on for "Not OK" instead of on for "OK" because destroying clipping paths is
		// an exceptional condition and we don't want to require normal filters to explicitly say they're OK.
		// Also, it is not necessary to turn this flag on if you can't take any paths at all.
		INPUT_NO_CLIPMASKS		= 0x20000;

	//AIStyleFilterFlags
	public static final int
		TYPE_DEFAULT		= 0,
		TYPE_PRE_EFFECT		= 1,
		TYPE_POST_EFFECT	= 2,
		TYPE_STROKE			= 3,
		TYPE_FILL			= 4;
	
	public static final int
		FLAG_NONE						= 0,
		FLAG_SPECIAL_GROU_PRE_FILTER 	= 0x010000,
		FLAG_HAS_SCALABLE_PARAMS 		= 0x020000,
		FLAG_USE_AUTO_RASTARIZE 		= 0x040000,
		FLAG_CAN_GENERATE_SVG_FILTER	= 0x080000;

	private String name;
	private String title;
	private int preferedInput;
	private int type;
	private int flags;
	private int majorVersion;
	private int minorVersion;
	private MenuItem menuItem = null;

	/**
	 * effects maps effectHandles to their wrappers.
	 */
	private static IntMap<LiveEffect> effects = new IntMap<LiveEffect>();
	private static ArrayList<LiveEffect> unusedEffects = null;

	/**
	 * Called from the native environment.
	 */
	protected LiveEffect(int handle, String name, String title,
			int preferedInput, int type, int flags, int majorVersion,
			int minorVersion) {
		super(handle);
		this.name = name;
		this.title = title;
		this.preferedInput = preferedInput;
		this.type = type;
		this.flags = flags;
		this.majorVersion = majorVersion;
		this.minorVersion = minorVersion;
	}

	/**
	 * @param name
	 * @param title
	 * @param preferedInput a combination of LiveEffect.INPUT_*
	 * @param type one of LiveEffect.TYPE_
	 * @param flags a combination of LiveEffect.FLAG_*
	 * @param majorVersion
	 * @param minorVersion
	 */
	public LiveEffect(String name, String title, int preferedInput, int type,
			int flags, int majorVersion, int minorVersion) {
		this(0, name, title, preferedInput, type, flags, majorVersion,
				minorVersion);

		ArrayList<LiveEffect> unusedEffects = getUnusedEffects();

		// Now see first whether there is an unusedEffect already that fits this
		// description
		int index = unusedEffects.indexOf(this);
		if (index >= 0) {
			// Found one, let's reuse it's handle and remove the old effect from
			// the list:
			LiveEffect effect = unusedEffects.get(index);
			handle = effect.handle;
			effect.handle = 0;
			unusedEffects.remove(index);
		} else {
			// No previously existing effect found, create a new one:
			handle = nativeCreate(name, title, preferedInput, type, flags,
					majorVersion, minorVersion);
		}

		if (handle == 0)
			throw new ScriptographerException("Unable to create LifeEffect.");

		effects.put(handle, this);
	}

	/**
	 * Same constructor, but name is used for title and name.
	 * 
	 * @param name
	 * @param preferedInput a combination of LiveEffect.INPUT_*
	 * @param type one of LiveEffect.TYPE_
	 * @param flags a combination of LiveEffect.FLAG_*
	 * @param majorVersion
	 * @param minorVersion
	 */
	public LiveEffect(String name, int preferedInput, int type, int flags,
			int majorVersion, int minorVersion) {
		this(name, name, preferedInput, type, flags, majorVersion, minorVersion);
	}

	private native int nativeCreate(String name, String title,
			int preferedInput, int type, int flags, int majorVersion,
			int minorVersion);

	/**
	 * "Removes" the effect. there is no real destroy for LiveEffects in
	 * Illustrator, so all it really does is remove the effect's menu item, if
	 * there is one. It keeps the effectHandle and puts itself in the list of
	 * unused effects
	 */
	public boolean remove() {
		// see whether we're still linked:
		if (effects.get(handle) == this) {
			// if so remove it and put it to the list of unused effects, for later recycling
			effects.remove(handle);
			getUnusedEffects().add(this);
			if (menuItem != null)
				menuItem.remove();
			menuItem = null;
			return true;
		}
		return false;
	}

	public static void removeAll() {
		// As remove() modifies the map, using an iterator is not possible here:
		Object[] effects = LiveEffect.effects.values().toArray();
		for (int i = 0; i < effects.length; i++) {
			((LiveEffect) effects[i]).remove();
		}
	}

	private native MenuItem nativeAddMenuItem(String name, String category,
			String title);

	public MenuItem addMenuItem(String cateogry, String title) {
		if (menuItem == null) {
			menuItem = nativeAddMenuItem(name, cateogry, title);
			return menuItem;
		}
		return null;
	}

	public MenuItem getMenuItem() {
		return menuItem;
	}

	/*
	 * used for unusedEffects.indexOf in the constructor above
	 */
	public boolean equals(Object obj) {
		if (obj instanceof LiveEffect) {
			LiveEffect effect = (LiveEffect) obj;
			return name.equals(effect.name) &&
					title.equals(effect.title) &&
					preferedInput == effect.preferedInput &&
					type == effect.type &&
					flags == effect.flags &&
					majorVersion == effect.majorVersion &&
					minorVersion == effect.minorVersion;
		}
		return false;
	}

	private static ArrayList<LiveEffect> getUnusedEffects() {
		if (unusedEffects == null)
			unusedEffects = nativeGetEffects();
		return unusedEffects;
	}

	private static native ArrayList<LiveEffect> nativeGetEffects();

	/**
	 * Call only from onEditParameters!
	 */
	public native boolean updateParameters(Map parameters);

	/**
	 * Call only from onEditParameters!
	 */
	// TODO: is this still needed? difference to getMenuItem()?
	public native Object getMenuItem(Map parameters);

	// Callback functions:

	private Callable onEditParameters = null;
	
	public Callable getOnEditParameters() {
		return onEditParameters;
	}

	public void setOnEditParameters(Callable onEditParameters) {
		this.onEditParameters = onEditParameters;
	}

	protected void onEditParameters(Map parameters) throws Exception {
		if (onEditParameters != null)
			ScriptographerEngine.invoke(onEditParameters, this, parameters);
	}

	private Callable onCalculate = null;
	
	public Callable getOnCalculate() {
		return onCalculate;
	}
	
	public void setOnCalculate(Callable onCalculate) {
		this.onCalculate = onCalculate;
	}

	protected Item onCalculate(Map parameters, Item item) throws Exception {
		if (onCalculate != null) {
			Object ret = ScriptographerEngine.invoke(onCalculate, this, parameters, item);
			// it is only possible to either return the art itself or set the
			// art to null!
			// everything else semse to cause a illustrator crash

			// TODO: This is not correct handling:
			// Am 23.02.2005 um 18:53 schrieb Frank Stokes-Guinan:
			// When creating output art for the go message, the output art must
			// be a child of the same parent as the input art. It also must be
			// the only child of this parent, so if you create a copy of the
			// input art, work on it and attempt to return the copy as the
			// output art, you must make sure to dispose the original input art
			// first. It is not legal to create an item in an arbitrary
			// place and return that as the output art.

			return ret == item ? item : null;
		}
		return null;
	}

	private Callable onGetInputType = null;

	public Callable getOnGetInputType() {
		return onGetInputType;
	}

	public void setOnGetInputType(Callable onGetInputType) {
		this.onGetInputType = onGetInputType;
	}

	protected int onGetInputType(Map parameters, Item item) throws Exception {
		if (onGetInputType != null) {
			return ConversionUtils.toInt(ScriptographerEngine.invoke(
					onGetInputType, parameters, item));
			
		}
		return 0;
	}

	/**
	 * To be called from the native environment:
	 */
	@SuppressWarnings("unused")
	private static void onEditParameters(int handle, int parameters,
			int effectContext, boolean allowPreview) throws Exception {
		LiveEffect effect = getEffect(handle);
		if (effect != null) {
			// TODO: put these special values to the parameters for the duration of
			// the handler the parameter map then needs to be passed to
			// functions like updateParameters
			/*
			parameters.put("context", new Integer(effectContext));
			parameters.put("allowPreview", new Boolean(allowPreview));
			*/
			effect.onEditParameters(Dictionary.wrapHandle(parameters));
			/*
			parameters.remove("context");
			parameters.remove("allowPreview");
			*/
		}
	}

	/**
	 * To be called from the native environment:
	 */
	@SuppressWarnings("unused")
	private static int onCalculate(int handle, int parameters, Item item)
			throws Exception {
		LiveEffect effect = getEffect(handle);
		if (effect != null) {
			Item newArt = effect.onCalculate(Dictionary.wrapHandle(parameters), item);
			if (newArt != null)
				item = newArt;
		}
		// already return the handle to the native environment so it doesn't
		// need to access it there...
		return item.handle;
	}

	/**
	 * To be called from the native environment:
	 */
	@SuppressWarnings("unused")
	private static int onGetInputType(int handle, int parameters, Item item)
			throws Exception {
		LiveEffect effect = getEffect(handle);
		if (effect != null)
			return effect.onGetInputType(Dictionary.wrapHandle(parameters), item);
		return 0;
	}

	private static LiveEffect getEffect(int handle) {
		return effects.get(handle);
	}
}
