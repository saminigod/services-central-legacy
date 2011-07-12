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
 * The Original Code is Firefox Sync.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Richard Newman <rnewman@mozilla.com>
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

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/repository.js");
Cu.import("resource://services-sync/util.js");

const EXPORTED_SYMBOLS = ["Synchronizer"];

/**
 * A SynchronizerSession exchanges data between two RepositorySessions.
 * As with other kinds of session, this is a one-shot object.
 *
 * Grab a session for each of our repositories. Once both sessions are set
 * up, we pair invocations of fetchSince and store callbacks, switching
 * places once the first stream is done. Then we dispose of each session and
 * invoke our callback.
 *
 * Example usage:
 *
 *   let session = new SynchronizerSession(synchronizer);
 *   session.onInitialized = function (err) {
 *     // Add error handling here.
 *     session.synchronize();
 *   };
 *   session.onSynchronized = function (err) {
 *     // Rock on!
 *     callback(err);
 *   };
 *   session.init();
 */
function SynchronizerSession(synchronizer) {
  this.synchronizer = synchronizer;

  let level = Svc.Prefs.get("log.logger.synchronizersession");
  this._log = Log4Moz.repository.getLogger("Sync.SynchronizerSession");
  this._log.level = Log4Moz.Level[level];
}
SynchronizerSession.prototype = {
  sessionA:     null,
  sessionB:     null,
  synchronizer: null,
  timestampA:   null,
  timestampB:   null,

  /**
   * Override these two methods!
   */
  onInitialized: function onInitialized(error) {
    throw "Override onInitialized.";
  },

  onSynchronized: function onSynchronized(error) {
    throw "Override onSynchronized.";
  },

  /**
   * Override this if you want to terminate fetching, or apply
   * other recovery/etc. handling.
   */
  onFetchError: function onFetchError(error, record) {
    // E.g., return Repository.prototype.STOP;
  },

  /**
   * storeCallback doesn't admit any kind of control flow, so only bother
   * overriding this if you want to watch what's happening.
   */
  onStoreError: function onStoreError(error) {
  },

  fetchCallback: function fetchCallback(session, error, record) {
    if (error) {
      this._log.warn("Got error " + Utils.exceptionStr(error) +
                     " fetching. Invoking onFetchError for handling.");
      // Return the handler value, which allows the caller to do useful things
      // like abort.
      return this.onFetchError(error, record);
    }
    session.store(record);
  },

  /**
   * The two storeCallbacks are instrumental in switching sync direction and
   * actually finishing the sync. This is where the magic happens.
   */
  storeCallbackB: function storeCallbackB(error) {
    if (error != Repository.prototype.DONE) {
      // Hook for handling. No response channel yet.
      this.onStoreError(error);
      return;
    }
    this._log.debug("Done with records in storeCallbackB.");
    this._log.debug("Fetching from B into A.");
    // On to the next!
    this.sessionB.fetchSince(this.synchronizer.lastSyncB, this.fetchCallback.bind(this, this.sessionA));
  },

  storeCallbackA: function storeCallbackA(error) {
    this._log.debug("In storeCallbackA().");
    if (error != Repository.prototype.DONE) {
      this.onStoreError(error);
      return;
    }
    this._log.debug("Done with records in storeCallbackA.");
    this.finishSync();
  },

  sessionCallbackA: function sessionCallbackA(error, session) {
    this.sessionA = session;
    if (error) {
      this.onInitialized(error);
      return;
    }
    this.synchronizer.repositoryB.createSession(this.storeCallbackB.bind(this),
                                                this.sessionCallbackB.bind(this));
  },

  sessionCallbackB: function sessionCallbackB(error, session) {
    this.sessionB = session;
    this._log.debug("Session timestamps: A = " + this.sessionA.timestamp +
                    ", B = " + this.sessionB.timestamp);
    if (error) {
      return this.sessionA.dispose(function () {
        this.onInitialized(error);
      }.bind(this));
    }
    this.onInitialized();
  },

  /**
   * Dispose of both sessions and invoke onSynchronized.
   */
  finishSync: function finishSync() {
    this.sessionA.dispose(function (timestampA) {
      this.timestampA = timestampA;
      this.sessionB.dispose(function (timestampB) {
        this.timestampB = timestampB;
        // Finally invoke the output callback.
        this.onSynchronized(null);
      }.bind(this));
    }.bind(this));
  },

  /**
   * Initialize the two repository sessions, then invoke onInitialized.
   */
  init: function init() {
    this.synchronizer.repositoryA.createSession(this.storeCallbackA.bind(this),
                                                this.sessionCallbackA.bind(this));
  },

  /**
   * Assuming that two sessions have been initialized, sync, then clean up and
   * invoke onSynchronized.
   */
  synchronize: function synchronize() {
    this.sessionA.fetchSince(this.synchronizer.lastSyncA,
                             this.fetchCallback.bind(this, this.sessionB));
  }
};

/**
 * A Synchronizer exchanges data between two Repositories.
 *
 * It tracks whatever information is necessary to reify the syncing
 * relationship between these two sources/sinks: e.g., last sync time.
 */
function Synchronizer() {
  let level = Svc.Prefs.get("log.logger.synchronizer");
  this._log = Log4Moz.repository.getLogger("Sync.Synchronizer");
  this._log.level = Log4Moz.Level[level];
}
Synchronizer.prototype = {

  /**
   * Keep track of timestamps. These need to be persisted.
   */
  lastSyncA: 0,
  lastSyncB: 0,

  /**
   * Repositories to sync.
   *
   * The synchronizer will first sync from A to B and then from B to A.
   */
  repositoryA: null,
  repositoryB: null,

  /**
   * Do the stuff to the thing.
   */
  synchronize: function synchronize(callback) {
    this._log.debug("Entering Synchronizer.synchronize().");

    let session = new SynchronizerSession(this);
    session.onInitialized = function (error) {
      // Invoked with session as `this`.
      if (error) {
        this._log.warn("Error initializing SynchronizerSession: " +
                       Utils.exceptionStr(error));
        return callback(error);
      }
      this.synchronize();
    };
    session.onSynchronized = function (error) {
      // Invoked with session as `this`.
      if (error) {
        this._log.warn("Error during synchronization: " +
                       Utils.exceptionStr(error));
        return callback(error);
      }
      // Copy across the timestamps from within the session.
      this.synchronizer.lastSyncA = this.timestampA;
      this.synchronizer.lastSyncB = this.timestampB;
      callback();
    };
    session.init();
  },

  /**
   * Synchronize. This method blocks execution of the caller. It is deprecated
   * and solely kept for backward-compatibility.
   */
  sync: function sync() {
    Async.callSpinningly(this, this.synchronize);
  }
};


/**
 * Synchronize a Firefox engine to a Server11Collection.
 *
 * N.B., this class layers two accessors -- local and remote -- on top of the
 * undiscriminated pair of repositories exposed by Synchronizer.
 */
function EngineCollectionSynchronizer(name, local, remote) {
  Synchronizer.call(this);
  this.Name = name;
  this.name = name.toLowerCase();
  this.repositoryA = local;
  this.repositoryB = remote;
}
EngineCollectionSynchronizer.prototype = {
  __proto__: Synchronizer.prototype,

  /**
   * Convention.
   */
  get localRepository()  this.repositoryA,
  get serverRepository() this.repositoryB,

  /**
   * lastSync is a timestamp in server time.
   */
  get lastSync() {
    return parseFloat(Svc.Prefs.get(this.name + ".lastSync", "0"));
  },
  set lastSync(value) {
    // Reset the pref in-case it's a number instead of a string
    Svc.Prefs.reset(this.name + ".lastSync");
    // Store the value as a string to keep floating point precision
    Svc.Prefs.set(this.name + ".lastSync", value.toString());
  },

  /**
   * lastSyncLocal is a timestamp in local time.
   */
  get lastSyncLocal() {
    return parseInt(Svc.Prefs.get(this.name + ".lastSyncLocal", "0"), 10);
  },
  set lastSyncLocal(value) {
    // Store as a string because pref can only store C longs as numbers.
    Svc.Prefs.set(this.name + ".lastSyncLocal", value.toString());
  },
};
