/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Gecko Layout
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <bsmedberg@covad.net>
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsLayoutStylesheetCache_h__
#define nsLayoutStylesheetCache_h__

#include "nsICSSStyleSheet.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"

class nsIFile;

namespace mozilla {
namespace css {
class Loader;
}
}

class nsLayoutStylesheetCache
 : public nsIObserver
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static nsICSSStyleSheet* ScrollbarsSheet();
  static nsICSSStyleSheet* FormsSheet();
  static nsICSSStyleSheet* UserContentSheet();
  static nsICSSStyleSheet* UserChromeSheet();
  static nsICSSStyleSheet* UASheet();
  static nsICSSStyleSheet* QuirkSheet();

  static void Shutdown();

private:
  nsLayoutStylesheetCache();
  ~nsLayoutStylesheetCache();

  static void EnsureGlobal();
  void InitFromProfile();
  static void LoadSheetFile(nsIFile* aFile, nsCOMPtr<nsICSSStyleSheet> &aSheet);
  static void LoadSheet(nsIURI* aURI, nsCOMPtr<nsICSSStyleSheet> &aSheet,
                        PRBool aEnableUnsafeRules);

  static nsLayoutStylesheetCache* gStyleCache;
  static mozilla::css::Loader* gCSSLoader;
  nsCOMPtr<nsICSSStyleSheet> mScrollbarsSheet;
  nsCOMPtr<nsICSSStyleSheet> mFormsSheet;
  nsCOMPtr<nsICSSStyleSheet> mUserContentSheet;
  nsCOMPtr<nsICSSStyleSheet> mUserChromeSheet;
  nsCOMPtr<nsICSSStyleSheet> mUASheet;
  nsCOMPtr<nsICSSStyleSheet> mQuirkSheet;
};

#endif
