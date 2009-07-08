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
 * $Id$
 */
 
#include "stdHeaders.h"
#include "ScriptographerEngine.h"
#include "ScriptographerPlugin.h"
#include "aiGlobals.h"
#include "com_scriptographer_ai_Item.h"

/*
 * com.scriptographer.ai.Item
 */

bool Item_isValid(AIArtHandle art) {
#if kPluginInterfaceVersion < kAI12
	return sAIArt->ValidArt(art);
#else
	// TODO: If searchAllLayerLists is true, then this does a search through all layers in
	// all layer lists. Otherwise, it only does a search on the current layer list.
	return sAIArt->ValidArt(art, false);
#endif
}

short Item_getType(AIArtHandle art) {
	short type = -1;
	sAIArt->GetArtType(art, &type);
	return type;
}

short Item_getType(JNIEnv *env, jclass cls) {
	if (env->IsSameObject(cls, gEngine->cls_ai_Item)) {
		return kAnyArt;
	} else if (env->IsSameObject(cls, gEngine->cls_ai_Path)) {
		return kPathArt;
	} else if (env->IsSameObject(cls, gEngine->cls_ai_CompoundPath)) {
		return kCompoundPathArt;
	} else if (env->IsSameObject(cls, gEngine->cls_ai_Raster)) {
		return kRasterArt;
	} else if (env->IsSameObject(cls, gEngine->cls_ai_PlacedFile)) {
		return kPlacedArt;
	} else if (env->IsSameObject(cls, gEngine->cls_ai_PlacedSymbol)) {
		return kSymbolArt;
	} else if (env->IsAssignableFrom(cls, gEngine->cls_ai_TextItem)) {
		return kTextFrameArt;
	} else if (env->IsSameObject(cls, gEngine->cls_ai_Tracing)) {
		// special defined type for tracings, needs handling!
		return com_scriptographer_ai_Item_TYPE_TRACING;
	} else if (env->IsSameObject(cls, gEngine->cls_ai_Layer)) {
		// special defined type for layers, needs handling!
		return com_scriptographer_ai_Item_TYPE_LAYER;
	} else if (env->IsSameObject(cls, gEngine->cls_ai_Group)) {
		return kGroupArt;
	}
	return kUnknownArt;
	// TODO: make sure the above list contains all Item classes!
}

jboolean Item_hasChildren(AIArtHandle art) {
	// don't show the children of textPaths and pointText 
#if kPluginInterfaceVersion < kAI11
	short type = artGetType(art);
	return (type == kTextArt && artGetTextType(art) != kPointTextType) || (type != kTextPathArt);
#else
	return true;
#endif
}

jboolean Item_isLayer(AIArtHandle art) {
	ASBoolean isLayerGroup = false;
	sAIArt->IsArtLayerGroup(art, &isLayerGroup);
	return isLayerGroup;
}

void Item_filter(AIArtSet set, bool layerOnly) {
	// takes out all kUnknownArt, kTextRunArt, ... objs
	// removes layergroups as well
	long count;
	sAIArtSet->CountArtSet(set, &count);
	for (long i = count - 1; i >= 0; i--) {
		AIArtHandle art = NULL;
		if (!sAIArtSet->IndexArtSet(set, i, &art)) {
			short type = Item_getType(art);
			bool isLayer = Item_isLayer(art);
			if (type == kUnknownArt ||
#if kPluginInterfaceVersion < kAI11
				type == kTextRunArt ||
#endif
				(layerOnly && !isLayer || !layerOnly && isLayer)) {
				sAIArtSet->RemoveArtFromArtSet(set, art);
			}
		}
	}
}

AIArtSet Item_getSelected(bool filter) {
	AIArtSet set = NULL;
	if (!sAIArtSet->NewArtSet(&set)) {
		if (!sAIArtSet->SelectedArtSet(set)) {
			if (filter) {
				// Now filter out objects of which the parents are selected too
				long count;
				sAIArtSet->CountArtSet(set, &count);
				for (long i = count - 1; i >= 0; i--) {
					AIArtHandle art;
					if (!sAIArtSet->IndexArtSet(set, i, &art)) {
						long values;
						if (!sAIArt->GetArtUserAttr(art, kArtFullySelected, &values)
							&& !(values & kArtFullySelected)) {
							sAIArtSet->RemoveArtFromArtSet(set, art);
						} else {
							AIArtHandle parent = NULL;
							sAIArt->GetArtParent(art, &parent);
							if (parent != NULL && !Item_isLayer(parent)) {
								if (!sAIArt->GetArtUserAttr(parent, kArtFullySelected, &values)
									&& (values & kArtFullySelected))
									sAIArtSet->RemoveArtFromArtSet(set, art);
							}
						}
					}
				}
			}
		}
	}
	return set;
}

void Item_restoreSelected(AIArtSet set, bool dispose) {
	Document_deselectAll(true);
	// Select the previously selected objects:
	long count;
	if (set != NULL) {
		sAIArtSet->CountArtSet(set, &count);
		AIArtHandle art = NULL;
		for (long i = 0; i < count; i++) {
			if (!sAIArtSet->IndexArtSet(set, i, &art))
				sAIArt->SetArtUserAttr(art, kArtSelected, kArtSelected);
		}
		// Clean up
		if (dispose)
			sAIArtSet->DisposeArtSet(&set);
	}
}

AIArtSet Item_getSelected() {
	AIArtSet selected = NULL;
	if (!sAIArtSet->NewArtSet(&selected))
		sAIArtSet->SelectedArtSet(selected);
	return selected;
}	

void Item_activateDocument(JNIEnv *env, AIArtSet set) {
	long count = 0;
	// Walk through the items to find the first wrapped one and get the document handle from there.
	sAIArtSet->CountArtSet(set, &count);
	for (long i = 0; i < count; i++) {
		AIArtHandle art = NULL;
		if (!sAIArtSet->IndexArtSet(set, 0, &art)) {
			jobject obj = gEngine->getIfWrapped(env, art);
			if (obj != NULL) {
				gEngine->getDocumentHandle(env, obj, true);
				// Break since we're done.
				break;
			}
		}
	}
}

AIArtHandle Item_rasterize(AIArtSet set, AIRasterizeType type, float resolution, int antialiasing, float width, float height) {
	AIRasterizeSettings settings;
	if (type == -1) {
		// deterimine from document color model:
		short colorModel;
		sAIDocument->GetDocumentColorModel(&colorModel);
		switch (colorModel) {
			case kDocGrayColor:
				type = kRasterizeAGrayscale;
				break;
			case kDocRGBColor:
				type = kRasterizeARGB;
				break;
			case kDocCMYKColor:
				type = kRasterizeACMYK;
				break;
		}
	}
	settings.type = type;
	settings.resolution = resolution;
	settings.antialiasing = antialiasing;
	// TODO: Support options
	settings.options = kRasterizeOptionsNone;
	AIRealRect artBounds;
	sAIRasterize->ComputeArtBounds(set, &artBounds, false);
	artBounds.left = floor(artBounds.left);
	artBounds.bottom = floor(artBounds.bottom);
	if (width >= 0)
		artBounds.right = artBounds.left + width;
	if (height >= 0)
		artBounds.top = artBounds.bottom + height;
	artBounds.right = ceil(artBounds.right);
	artBounds.top = ceil(artBounds.top);
	AIArtHandle raster = NULL;
	// walk through the set and find the art that is blaced above all others:
	AIArtHandle top = NULL;
	long count;
	sAIArtSet->CountArtSet(set, &count);
	for (long i = count - 1; i >= 0; i--) {
		AIArtHandle art;
		if (!sAIArtSet->IndexArtSet(set, i, &art)) {
			if (top == NULL) {
				top = art;
			} else {
				short order;
				sAIArt->GetArtOrder(art, top, &order);
				if (order == kFirstBeforeSecond || order == kSecondInsideFirst)
					top = art;
			}
		}
	}
	sAIRasterize->Rasterize(set, &settings, &artBounds, kPlaceAbove, top, &raster, NULL);
	return raster;
}

AIArtHandle Item_rasterize(AIArtHandle art, AIRasterizeType type, float resolution, int antialiasing, float width, float height) {
	AIArtSet set;
	sAIArtSet->NewArtSet(&set);
	sAIArtSet->AddArtToArtSet(set, art);
	AIArtHandle raster = Item_rasterize(set, type, resolution, antialiasing, width, height);
	sAIArtSet->DisposeArtSet(&set);
	return raster;
}

AIArtHandle Item_getInsertionPoint(short *paintOrder, AIDocumentHandle doc) {
	// activate the focused document 
	Document_activate(doc);
	AIArtHandle art = NULL;
	*paintOrder = 0;
	ASBoolean editable = false;
	sAIArt->GetInsertionPoint(&art, paintOrder, &editable);
	if (!editable)
		throw new StringException("Unable to create item. The active layer is not editable.");
	return art;
}

/*
 * Commits and invalidates wrapped art objects.
 * if 'children' is set, it traverses the children recursively and commits and
 * invalidates them too.
 */
void Item_commit(JNIEnv *env, AIArtHandle art, bool invalidate, bool children) {
	jobject obj = gEngine->getIfWrapped(env, art);
	// only do this if it's wrapped!
	if (obj != NULL)
		gEngine->callVoidMethod(env, obj, gEngine->mid_ai_Item_commit, invalidate);
	if (children) {
		AIArtHandle child = NULL;
		sAIArt->GetArtFirstChild(art, &child);
		while (child != NULL) {
			Item_commit(env, child, invalidate, true);
			// Catch errors
			if (sAIArt->GetArtSibling(child, &child))
				child = NULL;
		}
	}
}

/*
 * Walks through the given dictionary and finds the key for art :)
 */
AIDictKey Item_getDictionaryKey(AIDictionaryRef dictionary, AIArtHandle art) {
	AIDictKey foundKey = NULL;
	AIDictionaryIterator iterator;
	if (!sAIDictionary->Begin(dictionary, &iterator)) {
		while (!sAIDictionaryIterator->AtEnd(iterator)) {
			AIDictKey key = sAIDictionaryIterator->GetKey(iterator);
			AIArtHandle curArt;
			if (!sAIDictionary->GetArtEntry(dictionary, key, &curArt) && art == curArt) {
				foundKey = key;
				break;
			}
			sAIDictionaryIterator->Next(iterator);
		}
		sAIDictionaryIterator->Release(iterator);
	}
	return foundKey;
}

void Item_clearArtHandles(AIArtHandle art) {
	AIDictionaryRef dict;
	if (!sAIArt->GetDictionary(art, &dict)) {
		sAIDictionary->DeleteEntry(dict, gEngine->m_artHandleKey);
		sAIDictionary->Release(dict);
	}
	// Clear children as well:
	AIArtHandle child = NULL;
	sAIArt->GetArtFirstChild(art, &child);
	if (child != NULL) {
		do {
			Item_clearArtHandles(child);
			if (sAIArt->GetArtSibling(child, &child))
				child = NULL;
		} while (child != NULL);
	}
}

AIArtHandle Item_copyTo(JNIEnv *env, AIArtHandle artSrc, AIDocumentHandle docSrc, AIDictionaryRef dictSrc, AIArtHandle artDst, AIDocumentHandle docDst, short paintOrder, bool commitFirst = true) {
	AIArtHandle res = NULL;
	AIRealMatrix matrix;
	bool transform = false;
	// Determine the shift in coordinate space between the two documents
	// by hardening and softening an identity transform, then reversing
	// the transform. This transformation then is applied to the resulting
	// item further down.
	if (docSrc != docDst) {
		Document_activate(docSrc);
		sAIRealMath->AIRealMatrixSetIdentity(&matrix);
		sAIHardSoft->AIRealMatrixHarden(&matrix);
		Document_activate(docDst);
		sAIHardSoft->AIRealMatrixSoften(&matrix);
		matrix.tx = -matrix.tx;
		matrix.ty = -matrix.ty;
		transform = true;
	} else {
		Document_activate(docDst);
	}
	// commit
	if (commitFirst)
		Item_commit(env, artSrc);
	if (dictSrc != NULL) {
		AIDictKey key = Item_getDictionaryKey(dictSrc, artSrc);
		if (key != NULL)
			sAIDictionary->CopyEntryToArt(dictSrc, key, paintOrder, artDst, &res);
	}
	if (res == NULL)
		sAIArt->DuplicateArt(artSrc, paintOrder, artDst, &res);
	if (res != NULL) {
		// TODO: Find out which additional options should be passed here:
		if (transform) 
			sAITransformArt->TransformArt(res, &matrix, 1, kTransformObjects | kTransformChildren);
		// Duplicate art also duplicated the dictionary. Remove the artHandleKey from it, since it's
		// a new object that needs to be wrapped differently.
		// It appears that it can also happen that newly duplicated items receive dictionaries from already gone
		// items, e.g. after undo, at least in CS4. Clearing here gets rid of these problems...
		Item_clearArtHandles(res);
	}
	return res;
}

bool Item_move(JNIEnv *env, jobject obj, jobject item, short paintOrder) {
	try {
		if (item != NULL) {
			AIDocumentHandle docSrc, docDst;
			AIArtHandle artSrc = gEngine->getArtHandle(env, obj, true, &docSrc);
			AIArtHandle artDst = gEngine->getArtHandle(env, item, false, &docDst);
			// Commit source first
			Item_commit(env, artSrc);
			if (artSrc != NULL && artDst != NULL && artSrc != artDst) {
				// Simply try to reorder it
				if (artSrc != NULL && artDst != NULL) {
					// If art belongs to a dictionary, treat it differently
					AIDictionaryRef dictSrc = gEngine->getArtDictionaryHandle(env, obj);
					if (dictSrc != NULL) {
						AIDictKey key = Item_getDictionaryKey(dictSrc, artSrc);
						if (key != NULL) {
							if (!sAIDictionary->MoveEntryToArt(dictSrc, key, paintOrder, artDst, &artSrc)) {
								gEngine->changeArtHandle(env, obj, artSrc);
								return true;
							}
						}
					}
					
					// If we're in a different document:
					// move the art from one document to the other by moving it to
					// the the doc's dictionary first, then into the doc from there
					// this is the only way that seems to work...
					if (docSrc != docDst) {
						// Pass false for commitFirst since it was already commited above
						AIArtHandle res = Item_copyTo(env, artSrc, docSrc, NULL, artDst, docDst, paintOrder, false);
						if (res != NULL) {
							gEngine->changeArtHandle(env, obj, res, docDst, NULL);
							// now remove the original object in docDst. Moving does not work directly
							// so this seems to be the most elegant way of handling this
							// TODO: Since Item_copyTo now seems to work with sAIArt->DuplicateArt for this,
							// check again if normal oving across documents might work.
							Document_activate(docSrc);
							sAIArt->DisposeArt(artSrc);
							return true;
						}
					}
					// Simply reorder
					if (!sAIArt->ReorderArt(artSrc, paintOrder, artDst))
						return true;
				}
			}
		}
	} EXCEPTION_CONVERT(env);
	return false;
}

short Item_getOrder(JNIEnv *env, jobject obj1, jobject obj2) {
	// No need to activate docs for only retrieving information!
	AIArtHandle art1 = gEngine->getArtHandle(env, obj1);
	AIArtHandle art2 = gEngine->getArtHandle(env, obj2);
	short order;
	sAIArt->GetArtOrder(art1, art2, &order);
	return order;
}

/*
 * int nativeCreate(short type)
 */
JNIEXPORT jint JNICALL Java_com_scriptographer_ai_Item_nativeCreate(JNIEnv *env, jclass cls, jshort type) {
	AIArtHandle art = NULL;
	try {
		// if type is set to the self defined TYPE_LAYER, create a layer and return the wrapped art group object instead:
		if (type == com_scriptographer_ai_Item_TYPE_LAYER) { // Create a layer
			// Make sure we're switching to the right doc (gCreationDoc)
			Document_activate();
			// place it above the active layer, or all others if none is active:
			AILayerHandle currentLayer = NULL;
			sAILayer->GetCurrentLayer(&currentLayer);
			AILayerHandle layer = NULL;
			sAILayer->InsertLayer(currentLayer, currentLayer != NULL ? kPlaceAbove : kPlaceAboveAll, &layer);
			if (layer != NULL)
				sAIArt->GetFirstArtOfLayer(layer, &art);
			if (art == NULL)
				throw new StringException("Unable to create layer. Make sure there is an open document.");
		} else { // create a normal art object
			short paintOrder;
			AIArtHandle artInsert = Item_getInsertionPoint(&paintOrder);
			// try to create in the active layer
			sAIArt->NewArt(type, paintOrder, artInsert, &art);
			if (art == NULL)
				throw new StringException("Unable to create item. Make sure there is an open document.");
		}
	} EXCEPTION_CONVERT(env);
	return (jint) art;
}

/*
 * void finalize()
 */
JNIEXPORT void JNICALL Java_com_scriptographer_ai_Item_finalize(JNIEnv *env, jobject obj) {
	try {
		AIDictionaryRef dictionary = gEngine->getArtDictionaryHandle(env, obj);
		if (dictionary != NULL) {
			sAIDictionary->Release(dictionary);
		}
	} EXCEPTION_CONVERT(env);
}

/*
 * boolean nativeRemove(int handle, int docHandle, jint dictionaryHandle)
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_nativeRemove(JNIEnv *env, jobject obj, jint handle, jint docHandle, jint dictionaryHandle) {
	try {
		Document_activate((AIDocumentHandle) docHandle);
		AIArtHandle art = (AIArtHandle) handle;
		// treat the object differently if it's in a dictionary than in the
		// normal artwork tree of the document:
		AIDictionaryRef dictionary = (AIDictionaryRef) dictionaryHandle;
		if (dictionary != NULL) {
			AIDictKey key = Item_getDictionaryKey(dictionary, art);
			if (key != NULL) {
				sAIDictionary->DeleteEntry(dictionary, key);
				return true;
			}
		}
		if (Item_isLayer(art)) {
			AILayerHandle layer;
			sAIArt->GetLayerOfArt(art, &layer);
			if (!sAILayer->DeleteLayer(layer))
				return true;
		} else {
			if (!sAIArt->DisposeArt(art))
				return true;
		}
	} EXCEPTION_CONVERT(env);
	return false;
}

/*
 * com.scriptographer.ai.Item copyTo(com.scriptographer.ai.Document document)
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_copyTo__Lcom_scriptographer_ai_Document_2(JNIEnv *env, jobject obj, jobject document) {
	try {
		// src
		AIDocumentHandle docSrc = NULL;
	    AIArtHandle artSrc = gEngine->getArtHandle(env, obj, false, &docSrc);
		AIDictionaryRef dictSrc = gEngine->getArtDictionaryHandle(env, obj);
		// dst: from insertion point
		AIDocumentHandle docDst = gEngine->getDocumentHandle(env, document);
		Document_activate(docDst);
		short paintOrder;
		AIArtHandle artDst = Item_getInsertionPoint(&paintOrder);
		// copy
		AIArtHandle copy = Item_copyTo(env, artSrc, docSrc, dictSrc, artDst, docDst, paintOrder);
		if (copy != NULL)
			return gEngine->wrapArtHandle(env, copy, docDst);
	} EXCEPTION_CONVERT(env);
	return NULL;
}

/*
 * com.scriptographer.ai.Item copyTo(com.scriptographer.ai.Item art)
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_copyTo__Lcom_scriptographer_ai_Item_2(JNIEnv *env, jobject obj, jobject item) {
	try {
		// src & dst
		AIDocumentHandle docSrc, docDst;
	    AIArtHandle artSrc = gEngine->getArtHandle(env, obj, false, &docSrc);
	    AIArtHandle artDst = gEngine->getArtHandle(env, item, false, &docDst);
		AIDictionaryRef dictSrc = gEngine->getArtDictionaryHandle(env, obj);
		// copy
		AIArtHandle copy = Item_copyTo(env, artSrc, docSrc, dictSrc, artDst, docDst, kPlaceInsideOnTop);
		if (copy != NULL)
			return gEngine->wrapArtHandle(env, copy, docDst);
	} EXCEPTION_CONVERT(env);
	return NULL;
}

/*
 * com.scriptographer.ai.Item getFirstChild()
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_getFirstChild(JNIEnv *env, jobject obj) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj);
		if (Item_hasChildren(art)) {
			AIArtHandle child = NULL;
			sAIArt->GetArtFirstChild(art, &child);
			if (child != NULL)
				return gEngine->wrapArtHandle(env, child, gEngine->getDocumentHandle(env, obj));
		}
	} EXCEPTION_CONVERT(env);
	return NULL;
}

/*
 * com.scriptographer.ai.Item getLastChild()
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_getLastChild(JNIEnv *env, jobject obj) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj);
		if (Item_hasChildren(art)) {
			AIArtHandle child = NULL;
			sAIArt->GetArtLastChild(art, &child);
			if (child != NULL)
				return gEngine->wrapArtHandle(env, child, gEngine->getDocumentHandle(env, obj));
		}
	} EXCEPTION_CONVERT(env);
	return NULL;
}

/*
 * com.scriptographer.ai.Item getNextSibling()
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_getNextSibling(JNIEnv *env, jobject obj) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj);
		AIArtHandle child = NULL;
		sAIArt->GetArtSibling(art, &child);
		if (child != NULL)
			return gEngine->wrapArtHandle(env, child, gEngine->getDocumentHandle(env, obj));
	} EXCEPTION_CONVERT(env);
	return NULL;
}

/*
 * com.scriptographer.ai.Item getPreviousSibling()
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_getPreviousSibling(JNIEnv *env, jobject obj) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj);
		AIArtHandle child = NULL;
		sAIArt->GetArtPriorSibling(art, &child);
		if (child != NULL)
			return gEngine->wrapArtHandle(env, child, gEngine->getDocumentHandle(env, obj));
	} EXCEPTION_CONVERT(env);
	return NULL;
}

/*
 * com.scriptographer.ai.Item getParent()
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_getParent(JNIEnv *env, jobject obj) {
	jobject res = NULL;
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj);
		AIArtHandle parent = NULL;
		sAIArt->GetArtParent(art, &parent);
		if (parent != NULL)
			res = gEngine->wrapArtHandle(env, parent, gEngine->getDocumentHandle(env, obj));
	} EXCEPTION_CONVERT(env);
	return res;
}

/*
 * com.scriptographer.ai.Layer getLayer()
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_getLayer(JNIEnv *env, jobject obj) {
	jobject res = NULL;
	try {
		AIArtHandle art = gEngine->getArtHandle(env, obj);
		// If this is already a layer, get the layer of it's parent, since getLayer would point
		// to itself otherwise (getLayer is supposed to return the layer the item is nested in).
		if (Item_isLayer(art))
			sAIArt->GetArtParent(art, &art);
		AILayerHandle layer = NULL;
		sAIArt->GetLayerOfArt(art, &layer);
		if (layer != NULL)
			res = gEngine->wrapLayerHandle(env, layer, gEngine->getDocumentHandle(env, obj));
	} EXCEPTION_CONVERT(env);
	return res;
}

/*
 * com.scriptographer.ai.Rectangle nativeGetBounds()
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_nativeGetBounds(JNIEnv *env, jobject obj) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj, true);
		// Commit pending changes first, since they might influence the bounds
		Item_commit(env, art);
		AIRealRect rt;
	    sAIArt->GetArtTransformBounds(art, NULL, kVisibleBounds | kNoStrokeBounds | kNoExtendedBounds | kExcludeGuideBounds, &rt);
	    return gEngine->convertRectangle(env, &rt);
	} EXCEPTION_CONVERT(env);
	return NULL;
}

/*
 * com.scriptographer.ai.Rectangle getStrokeBounds()
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_getStrokeBounds(JNIEnv *env, jobject obj) {
	try {
		AIRealRect rt;
	    AIArtHandle art = gEngine->getArtHandle(env, obj);
		// Commit pending changes first, since they might influence the bounds
		Item_commit(env, art);
	    sAIArt->GetArtTransformBounds(art, NULL, kVisibleBounds | kExcludeGuideBounds, &rt);
	    return gEngine->convertRectangle(env, &rt);
	} EXCEPTION_CONVERT(env);
	return NULL;
}

/*
 * com.scriptographer.ai.Rectangle getControlBounds()
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_getControlBounds(JNIEnv *env, jobject obj) {
	try {
		AIRealRect rt;
	    AIArtHandle art = gEngine->getArtHandle(env, obj);
		// Commit pending changes first, since they might influence the bounds
		Item_commit(env, art);
	    sAIArt->GetArtTransformBounds(art, NULL, kVisibleBounds | kControlBounds | kExcludeGuideBounds, &rt);
	    return gEngine->convertRectangle(env, &rt);
	} EXCEPTION_CONVERT(env);
	return NULL;
}

/*
 * com.scriptographer.ai.Point nativeGetPosition()
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_nativeGetPosition(JNIEnv *env, jobject obj) {
	try {
		AIRealRect rt;
	    AIArtHandle art = gEngine->getArtHandle(env, obj);
		// Commit pending changes first, since they might influence the bounds
		Item_commit(env, art);
		// Return the center point of the bounds
		// TODO: maybe find a way to define and store reg points per art objects?
	    sAIArt->GetArtTransformBounds(art, NULL, kVisibleBounds | kNoStrokeBounds | kNoExtendedBounds | kExcludeGuideBounds, &rt);
		DEFINE_POINT(pt, (rt.left + rt.right) / 2.0, (rt.top + rt.bottom) / 2.0);
	    return gEngine->convertPoint(env, &pt);
	} EXCEPTION_CONVERT(env);
	return NULL;
}

/*
 * boolean isCenterVisible()
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_isCenterVisible(JNIEnv *env, jobject obj) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj);
		AIBoolean visible;
		if (!sAIArt->GetArtCenterPointVisible(art, &visible))
			return visible;
	} EXCEPTION_CONVERT(env);
	return false;
}

/*
 * void setCenterVisible(boolean visible)
 */
JNIEXPORT void JNICALL Java_com_scriptographer_ai_Item_setCenterVisible(JNIEnv *env, jobject obj, jboolean visible) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj, true);
		sAIArt->SetArtCenterPointVisible(art, visible);
	} EXCEPTION_CONVERT(env);
}

/*
 * void nativeSetAttribute(long attribute, boolean value)
 */
JNIEXPORT void JNICALL Java_com_scriptographer_ai_Item_nativeSetAttribute(JNIEnv *env, jobject obj, jint attribute, jboolean value) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj, true);
		sAIArt->SetArtUserAttr(art, attribute, value ? attribute : 0);
    } EXCEPTION_CONVERT(env);
}

/*
 * boolean nativeGetAttribute(long attribute)
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_nativeGetAttribute(JNIEnv *env, jobject obj, jint attribute) {
	try {
		AIArtHandle art = gEngine->getArtHandle(env, obj);
		long values;
		sAIArt->GetArtUserAttr(art, attribute, &values);
		return values & attribute;
    } EXCEPTION_CONVERT(env);
	return false;
}

/*
 * boolean isEditable()
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_isEditable(JNIEnv *env, jobject obj) {
	ASBoolean res = false;
	try {
		AIArtHandle art = gEngine->getArtHandle(env, obj);
		long values;
		// First check that the art is not hidden or locked
		if (!sAIArt->GetArtUserAttr(art, kArtHidden | kArtLocked, &values) &&
			!values) {
			AIArtHandle prevArt;
			short order;
			ASBoolean editable;
			// Use Get / SetInsertionPoint to find out if object's parent is editable or not
			if (!sAIArt->GetInsertionPoint(&prevArt, &order, &editable)) {
				AIArtHandle checkArt;
				if (!sAIArt->GetArtParent(art, &checkArt)) {
					// Layers do not have parents
					if (checkArt == NULL)
						checkArt = art;
					AIArtHandle curArt;
					if (!sAIArt->SetInsertionPoint(checkArt) &&
						!sAIArt->GetInsertionPoint(&curArt, &order, &editable)) {
						res = editable;
					}
					// Set old insertion point again
					sAIArt->SetInsertionPoint(prevArt);
				}
			}
		}
	} EXCEPTION_CONVERT(env);
	return res;
}

/*
 * int getBlendMode()
 */
JNIEXPORT jint JNICALL Java_com_scriptographer_ai_Item_nativeGetBlendMode(JNIEnv *env, jobject obj) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj);
		return sAIBlendStyle->GetBlendingMode(art);
	} EXCEPTION_CONVERT(env);
	return 0;
}

/*
 * void nativeSetBlendMode(int mode)
 */
JNIEXPORT void JNICALL Java_com_scriptographer_ai_Item_nativeSetBlendMode(JNIEnv *env, jobject obj, jint mode) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj, true);
		sAIBlendStyle->SetBlendingMode(art, (AIBlendingMode) mode);
	} EXCEPTION_CONVERT(env);
}

/*
 * float getOpacity()
 */
JNIEXPORT jfloat JNICALL Java_com_scriptographer_ai_Item_getOpacity(JNIEnv *env, jobject obj) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj);
		return sAIBlendStyle->GetOpacity(art);
	} EXCEPTION_CONVERT(env);
	return 0.0;
}

/*
 * void setOpacity(float opacity)
 */
JNIEXPORT void JNICALL Java_com_scriptographer_ai_Item_setOpacity(JNIEnv *env, jobject obj, jfloat opacity) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj, true);
		sAIBlendStyle->SetOpacity(art, opacity);
	} EXCEPTION_CONVERT(env);
}

/*
 * boolean getIsolated()
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_getIsolated(JNIEnv *env, jobject obj) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj);
		return sAIBlendStyle->GetIsolated(art);
	} EXCEPTION_CONVERT(env);
	return false;
}

/*
 * void setIsolated(boolean isolated)
 */
JNIEXPORT void JNICALL Java_com_scriptographer_ai_Item_setIsolated(JNIEnv *env, jobject obj, jboolean isolated) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj, true);
		sAIBlendStyle->SetIsolated(art, isolated);
	} EXCEPTION_CONVERT(env);
}

/*
 * jint nativeGetKnockout(boolean inherited)
 */
JNIEXPORT jint JNICALL Java_com_scriptographer_ai_Item_nativeGetKnockout(JNIEnv *env, jobject obj, jboolean inherited) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj);
		return inherited ? sAIBlendStyle->GetInheritedKnockout(art) : sAIBlendStyle->GetKnockout(art);
	} EXCEPTION_CONVERT(env);
	return false;
}

/*
 * void nativeSetKnockout(int knockout)
 */
JNIEXPORT void JNICALL Java_com_scriptographer_ai_Item_nativeSetKnockout(JNIEnv *env, jobject obj, jint knockout) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj, true);
		sAIBlendStyle->SetKnockout(art, (AIKnockout) knockout);
	} EXCEPTION_CONVERT(env);
}

/*
 * boolean getAlphaIsShape()
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_getAlphaIsShape(JNIEnv *env, jobject obj) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj);
		return sAIBlendStyle->GetAlphaIsShape(art);
	} EXCEPTION_CONVERT(env);
	return false;
}

/*
 * void setAlphaIsShape(boolean isAlpha)
 */
JNIEXPORT void JNICALL Java_com_scriptographer_ai_Item_setAlphaIsShape(JNIEnv *env, jobject obj, jboolean isAlpha) {
	try {
	    AIArtHandle art = gEngine->getArtHandle(env, obj, true);
		sAIBlendStyle->SetAlphaIsShape(art, isAlpha);
	} EXCEPTION_CONVERT(env);
}

/*
 * java.lang.String getName()
 */
JNIEXPORT jstring JNICALL Java_com_scriptographer_ai_Item_getName(JNIEnv *env, jobject obj) {
	try {
		AIArtHandle art = gEngine->getArtHandle(env, obj);
#if kPluginInterfaceVersion < kAI12
		char name[1024];
		if (!sAIArt->GetArtName(art, name, 1024, NULL)) {
#else
		ai::UnicodeString name;
		if (!sAIArt->GetArtName(art, name, NULL)) {
#endif
			return gEngine->convertString(env, name);
		}
	} EXCEPTION_CONVERT(env);
	return NULL;
}

/*
 * void setName(java.lang.String name)
 */
JNIEXPORT void JNICALL Java_com_scriptographer_ai_Item_setName(JNIEnv *env, jobject obj, jstring name) {
	try {
		// intersting: setting name does not need document to be active
		AIArtHandle art = gEngine->getArtHandle(env, obj);
#if kPluginInterfaceVersion < kAI12
		char *str = gEngine->convertString(env, name);
		sAIArt->SetArtName(art, str);
		delete str;
#else
		ai::UnicodeString str = gEngine->convertString_UnicodeString(env, name);
		sAIArt->SetArtName(art, str);
#endif
	} EXCEPTION_CONVERT(env);
}

/*
 * boolean isDefaultName()
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_isDefaultName(JNIEnv *env, jobject obj) {
	ASBoolean isDefaultName = true;
	try {
		AIArtHandle art = gEngine->getArtHandle(env, obj);
#if kPluginInterfaceVersion < kAI12
		// at least one byte for the name needs to be specified, otherwise this
		// doesn't work:
		char name;
		sAIArt->GetArtName(art, &name, 1, &isDefaultName);
#else
		ai::UnicodeString name;
		sAIArt->GetArtName(art, name, &isDefaultName);
#endif
	} EXCEPTION_CONVERT(env);
	return isDefaultName;
}

/*
 * boolean appendTop(com.scriptographer.ai.Item item)
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_appendTop(JNIEnv *env, jobject obj, jobject item) {
	try {
		return Item_move(env, item, obj, kPlaceInsideOnTop);
	} EXCEPTION_CONVERT(env);
	return false;
}

/*
 * boolean appendBottom(com.scriptographer.ai.Item item)
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_appendBottom(JNIEnv *env, jobject obj, jobject item) {
	try {
		return Item_move(env, item, obj, kPlaceInsideOnBottom);
	} EXCEPTION_CONVERT(env);
	return false;
}

/*
 * boolean isAbove(com.scriptographer.ai.Item item)
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_isAbove(JNIEnv *env, jobject obj, jobject item) {
	try {
		return Item_getOrder(env, obj, item) == kFirstBeforeSecond;
	} EXCEPTION_CONVERT(env);
	return false;
}

/*
 * boolean isBelow(com.scriptographer.ai.Item item)
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_isBelow(JNIEnv *env, jobject obj, jobject item) {
	try {
		return Item_getOrder(env, obj, item) == kSecondBeforeFirst;
	} EXCEPTION_CONVERT(env);
	return false;
}

/*
 * boolean isDescendant(com.scriptographer.ai.Item item)
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_isDescendant(JNIEnv *env, jobject obj, jobject item) {
	try {
		return Item_getOrder(env, obj, item) == kFirstInsideSecond;
	} EXCEPTION_CONVERT(env);
	return false;
}

/*
 * boolean isAncestor(com.scriptographer.ai.Item item)
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_isAncestor(JNIEnv *env, jobject obj, jobject item) {
	try {
		return Item_getOrder(env, obj, item) == kSecondInsideFirst;
	} EXCEPTION_CONVERT(env);
	return false;
}
	
/*
 * boolean moveAbove(com.scriptographer.ai.Item art)
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_moveAbove(JNIEnv *env, jobject obj, jobject item) {
	return Item_move(env, obj, item, kPlaceAbove);
}

/*
 * boolean moveBelow(com.scriptographer.ai.Item art)
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_moveBelow(JNIEnv *env, jobject obj, jobject item) {
	return Item_move(env, obj, item, kPlaceBelow);
}

/*
 * void nativeTransform(com.scriptographer.ai.Matrix matrix, int scaleFlags)
 */
JNIEXPORT void JNICALL Java_com_scriptographer_ai_Item_nativeTransform(JNIEnv *env, jobject obj, jobject matrix, jint flags) {
	try {
		AIArtHandle art = gEngine->getArtHandle(env, obj, true);
		AIRealMatrix mx;
		gEngine->convertMatrix(env, matrix, &mx);
/*
		// Modify the matrix so that it 'acts' on the center of the selected object
		// TODO: Introduce reg points?
		AIRealRect bounds;
	    sAIArt->GetArtTransformBounds(art, NULL, kVisibleBounds | kNoStrokeBounds | kNoExtendedBounds | kExcludeGuideBounds, &bounds);
		DEFINE_POINT(center,
			(bounds.left + bounds.right) / 2,
			(bounds.top + bounds.bottom) / 2);

		AIRealMatrix m;
		sAIRealMath->AIRealMatrixSetTranslate(&m, -center.h, -center.v);
		sAIRealMath->AIRealMatrixConcat(&m, &mx, &m);
		sAIRealMath->AIRealMatrixConcatTranslate(&m, center.h, center.v);
*/
		// According to adobe sdk manual: linescale = sqrt(scaleX) * sqrt(scaleY)
		AIReal sx, sy;
		sAIRealMath->AIRealMatrixGetScale(&mx, &sx, &sy);
		AIReal lineScale = sAIRealMath->AIRealSqrt(sx) * sAIRealMath->AIRealSqrt(sy);
		Item_commit(env, art, true, flags & kTransformChildren);
		sAITransformArt->TransformArt(art, &mx, lineScale, flags);
	} EXCEPTION_CONVERT(env);
}

/*
 * com.scriptographer.ai.Raster nativeRasterize(int type, float resolution, int antialiasing, float width, float height)
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_nativeRasterize__IFIFF(JNIEnv *env, jobject obj, jint type, jfloat resolution, jint antialiasing, jfloat width, jfloat height) {
	try {
		AIArtHandle art = gEngine->getArtHandle(env, obj, true);
		AIArtHandle raster = Item_rasterize(art, (AIRasterizeType) type, resolution, antialiasing, width, height);
		if (raster != NULL) {
			// No need to pass document since we're activating document in getArtHandle
			return gEngine->wrapArtHandle(env, raster);
		}
	} EXCEPTION_CONVERT(env);
	return NULL;
}

/*
 * com.scriptographer.ai.Raster nativeRasterize(com.scriptographer.ai.Item[] items, int type, float resolution, int antialiasing, float width, float height)
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_nativeRasterize___3Lcom_scriptographer_ai_Item_2IFIFF(JNIEnv *env, jclass cls, jobjectArray items, jint type, jfloat resolution, jint antialiasing, jfloat width, jfloat height) {
	try {
		AIArtSet set = gEngine->convertItemSet(env, items, true);
		AIArtHandle raster = Item_rasterize(set, (AIRasterizeType) type, resolution, antialiasing, width, height);
		if (raster != NULL) {
			// It's ok not to not pass document here, since the method calling nativeRasterize makes sure the right one is active
			return gEngine->wrapArtHandle(env, raster);
		}
	} EXCEPTION_CONVERT(env);
	return NULL;
}

/*
 * void nativeDraw(com.scriptographer.ui.Image image, int width, int height)
 */
JNIEXPORT void JNICALL Java_com_scriptographer_ai_Item_nativeDraw(JNIEnv *env, jobject obj, jobject imageObj, jint width, jint height) {
	try {
		AIArtHandle art = gEngine->getArtHandle(env, obj, true);

		Item_commit(env, art);
		AIRealRect rt;
	    sAIArt->GetArtTransformBounds(art, NULL, kVisibleBounds | kNoStrokeBounds | kNoExtendedBounds | kExcludeGuideBounds, &rt);

		ADMImageRef image = gEngine->getImageHandle(env, imageObj);
		ADMDrawerRef drawer = sADMImage->BeginADMDrawer(image);
		ADMAGMPortPtr port = sADMDrawer->GetAGMPort(drawer);
		
		AIDrawArtFlags flags = kAIDrawArtPreviewMask;
		AIDrawArtData drawData;
		drawData.version = kAIDrawArtVersion;
		drawData.flags = flags;
		drawData.type = kAIDrawArtAGMPortOutputV6;
		drawData.origin.h = rt.left;
		drawData.origin.v = rt.top;
		sAIRealMath->AIRealMatrixSetIdentity(&drawData.matrix);
		drawData.art = art;
		AIRealRect &clip = drawData.destClipRect;
		clip.left = clip.top = 0;
		clip.right = width;
		clip.bottom = height;
//		clip.right = rt.right - rt.left;
//		clip.bottom = rt.top - rt.bottom;
		drawData.eraseDestClipRect = true;
		drawData.interruptedArt = NULL;
		drawData.greekThreshold = -1;
		drawData.output.port.port = port;
		AIRect &rect = drawData.output.port.portBounds;
		rect.left = rect.top = 0;
		rect.right = width;
		rect.bottom = height;
#if kPluginInterfaceVersion >= kAI12
		// TODO: implement for CS
		AIColorConvertOptions drawOptions;
		// sAIDrawArt->BeginDrawArt(&drawData, drawOptions, flags); 
		sAIDrawArt->DrawArt(&drawData, drawOptions); 
		// sAIDrawArt->EndDrawArt(&drawData, drawOptions); 
		sADMImage->EndADMDrawer(image);
#endif
	} EXCEPTION_CONVERT(env);
}

/*
 * com.scriptographer.ai.Item nativeExpand(int flags, int steps)
 */
JNIEXPORT jobject JNICALL Java_com_scriptographer_ai_Item_nativeExpand(JNIEnv *env, jobject obj, jint flags, jint steps) {
	try {
		AIArtHandle art = gEngine->getArtHandle(env, obj, true);
		// commit pending changes first, before native expand is called!
		Item_commit(env, art);
		// store old selected items:
		AIArtHandle **selected = NULL;
		long numSelected = 0;
		sAIMatchingArt->GetSelectedArt(&selected, &numSelected);
		Document_deselectAll();
		// now selected the art to be expanded, so the result is selected too:
		sAIArt->SetArtUserAttr(art, kArtSelected, kArtSelected);
		// expand
		sAIExpand->Expand(art, flags, steps);
		// now get the new selection, to find the result:
		AIArtSet result = NULL;
		AIArtHandle res = NULL;
		if (!sAIArtSet->NewArtSet(&result) &&
			!sAIArtSet->SelectedArtSet(result)) {
			sAIArtSet->IndexArtSet(result, 0, &res);
			sAIArtSet->DisposeArtSet(&result);
			sAIArt->SetArtUserAttr(res, kArtSelected, 0);
		}
		// deselect again
		Document_deselectAll(true);
		// select the previously selected objects:
		for (long i = 0; i < numSelected; i++)
			sAIArt->SetArtUserAttr((*selected)[i], kArtSelected, kArtSelected);
		sAIMDMemory->MdMemoryDisposeHandle((void **) selected);
		// No need to pass document since we're activating document in getArtHandle
		return gEngine->wrapArtHandle(env, res);
	} EXCEPTION_CONVERT(env);
	return NULL;
}

/*
 * boolean isValid()
 */
JNIEXPORT jboolean JNICALL Java_com_scriptographer_ai_Item_isValid(JNIEnv *env, jobject obj) {
	try {
		AIArtHandle art = gEngine->getArtHandle(env, obj, true);
		return Item_isValid(art);
	} catch (ScriptographerException *e) {
		// Do not report, just return false
		delete e;
	}
	return false;
}

/*
 * void activate()
 */
JNIEXPORT void JNICALL Java_com_scriptographer_ai_Item_activate(JNIEnv *env, jobject obj) {
	try {
		AIArtHandle art = gEngine->getArtHandle(env, obj, true);
		sAIArt->SetInsertionPoint(art);
	} EXCEPTION_CONVERT(env);
}

/*
 * int nativeGetData()
 */
JNIEXPORT jint JNICALL Java_com_scriptographer_ai_Item_nativeGetData(JNIEnv *env, jobject obj) {
	AIDictionaryRef dictionary = NULL;
	try {
		AIArtHandle art = gEngine->getArtHandle(env, obj);
		sAIArt->GetDictionary(art, &dictionary);
	} EXCEPTION_CONVERT(env);
	return (jint) dictionary;
}

/*
 * int getItemType()
 */
JNIEXPORT jint JNICALL Java_com_scriptographer_ai_Item_getItemType__(JNIEnv *env, jobject obj) {
	try {
		return Item_getType(gEngine->getArtHandle(env, obj));
	} EXCEPTION_CONVERT(env);
	return 0;
}

/*
 * int getItemType(java.lang.Class item)
 */
JNIEXPORT jint JNICALL Java_com_scriptographer_ai_Item_getItemType__Ljava_lang_Class_2(JNIEnv *env, jclass cls, jclass item) {
	try {
		return Item_getType(env, item);
	} EXCEPTION_CONVERT(env);
	return 0;
}
