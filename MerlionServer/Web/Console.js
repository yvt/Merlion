/**
 * Copyright (C) 2014 yvt <i@yvt.jp>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

$(function() {

	function bindList(parent, options)
	{
		var rows = {};
        var inserter = options.inserter;
        if (!inserter) {
            inserter = function(e) {
                e.appendTo(parent);
            };
        }

		return {
			update: function (data) {
				for (var key in rows)
					rows[key].alive = false;
				for (var key in data) {
					if (typeof rows[key] === 'undefined') {
                        rows[key] = {
                            key: key,
                            alive: true
                        };

                        rows[key].element = options.create(rows[key], data[key]);

                        inserter(rows[key].element);
                    } else {
                        rows[key].alive = true;
                        options.update(rows[key], data[key]);
                    }
				}
                var garbage = [];
                for (var key in rows)
                    if (!rows[key].alive)
                        garbage.push(key);
                for (var i = 0; i < garbage.length; ++i) {
                    var key = garbage[i];
                    var row = rows[key];
                    delete rows[key];
                    row.element.remove();
                }
			}
		};
	}	


    var versionTable = bindList($('#version-table'), {
        create: function (row, data) {
            var tr = row.tr = $('<tr>');
            row.dVer = $('<td class="cell-version-name">').appendTo(tr);
            row.dNumRooms = $('<td class="cell-num-rooms">').appendTo(tr);
            row.dNumClients = $('<td class="cell-num-clients">').appendTo(tr);
            row.dThrottle = $('<td class="cell-throttle">').appendTo(tr);
            row.dActions = $('<td class="cell-actions">').appendTo(tr);

            row.dThrottleSlider = $('<div>').appendTo(row.dThrottle)
            .slider({
                value: 0, min: 0, max: 1, step: 0.05,
                range: 'min', change: function(e, ui) {
                    var val = row.dThrottleSlider.slider('value');
                    if (val === row.lastValue)
                        return;

                    $.ajax({
                        url: '/version/throttle/set.json',
                        type: 'POST',
                        data: {
                            version: data.name,
                            throttle: val
                        },
                        success: function() {
                            row.lastValue = val;
                        },
                        error: handleAjaxApiError
                    });
                }
            });
            $('<button class="btn btn-danger btn-sm">')
            .text('Remove').appendTo(row.dActions)
            .click(function() {
                $.ajax({
                    url: '/version/destroy.json',
                    type: 'POST',
                    data: {
                        version: data.name
                    },
                    success: function() {
                        refreshAll()
                    },
                    error: handleAjaxApiError
                });
            });

            return this.update(row, data);
        },
        update: function (row, data) {
            row.dVer.text(data.name);
            row.dNumRooms.text(formatCount(data.numRooms) + (data.numRooms == 1 ? " room" : " rooms"));
            row.dNumClients.text(formatCount(data.numClients) + (data.numClients == 1 ? " client" : " clients"));
            row.lastValue = data.throttle;
            row.dThrottleSlider.slider('value', data.throttle);
            return row.tr;
        }
    });

    var nodeTable = bindList($('#server-table'), {
        create: function (row, data) {
            var tr = row.tr = $('<tr class="node">');
            row.dBody = $('<td>').appendTo(tr);
            row.dNumRooms = $('<td class="cell-num-rooms">').appendTo(tr);
            row.dNumClients = $('<td class="cell-num-clients">').appendTo(tr);

            row.dThrottleSlider = $('<div class="throttle">').appendTo(row.dBody)
            .slider({
                value: 0, min: 0, max: 1, step: 0.05,
                range: 'min', change: function(e, ui) {
                    var val = row.dThrottleSlider.slider('value');
                    if (val === row.lastValue)
                        return;

                    $.ajax({
                        url: '/node/throttle/set.json',
                        type: 'POST',
                        data: {
                            name: data.name,
                            throttle: val
                        },
                        success: function() {
                            row.lastValue = val;
                        },
                        error: handleAjaxApiError
                    });
                }
            });

            row.dHeader = $('<h3>').appendTo(row.dBody);
            row.dNodeName = $('<span>').appendTo(row.dHeader);
            row.dHost = $('<small>').appendTo(row.dHeader);

            var p1 = $('<p>').appendTo(row.dBody);
            row.dServer = $('<span>').appendTo(p1);
            row.dUpTime = $('<span>').appendTo(p1);

            // var p2 = $('<p>').appendTo(row.dBody);

            row.domainTable = bindList(this, {
                create: function (row, data) {
                    var tr = row.tr = $('<tr class="domain">');
                    row.dBody = $('<td class="cell-domain-name">').appendTo(tr);
                    row.dNumRooms = $('<td class="cell-num-rooms">').appendTo(tr);
                    row.dNumClients = $('<td class="cell-num-clients">').appendTo(tr);

                    row.dHeader = $('<h4>').appendTo(row.dBody);
                    row.dVer = $('<span>').appendTo(row.dHeader);
                    row.dUpTime = $('<small>').appendTo(row.dHeader);
                    return this.update(row, data);
                },
                update: function (row, data) {
                    row.dVer.text(data.version);
                    row.dUpTime.text(formatUpTime(data.uptime));
                    row.dNumRooms.text(formatCount(data.numRooms) + (data.numRooms == 1 ? " room" : " rooms"));
                    row.dNumClients.text(formatCount(data.numClients) + (data.numClients == 1 ? " client" : " clients"));
                    return row.tr;
                },
                inserter: function (e) {
                    row.tr.after(e);
                }
            });

            var self = this;
            setTimeout(function() {
                self.update(row, data);
            }, 0);

            return row.tr;
        },
        update: function (row, data) {
            row.dNodeName.text(data.name);
            row.dHost.text(data.host);
            row.dNumRooms.text(formatCount(data.numRooms) + (data.numRooms == 1 ? " room" : " rooms"));
            row.dNumClients.text(formatCount(data.numClients) + (data.numClients == 1 ? " client" : " clients"));
            row.dServer.text(data.server + ", ");
            row.dUpTime.text(formatUpTime(data.uptime) + " since node startup.");
            row.lastValue = data.throttle;
            row.dThrottleSlider.slider('value', data.throttle);
            row.domainTable.update(data.domains);
            return row.tr;
        }
    });


    function refreshStatus()
    {
        $.ajax({
            url: 'status.json', 
            dataType: 'json',
            success: function(data) {
                var versions = {};
                var domains = {};
                var nodes = {};
                $.each(data.versions, function() {
                    versions[this.name] = this;
                    this.numRooms = 0;
                    this.numClients = 0;
                }); 
                $.each(data.nodes, function() {
                    var ndomains = this.domains;
                    for (var verName in ndomains) {
                        var ver = versions[verName];
                        var dom = ndomains[verName];
                        if (typeof ver !== 'undefined') {
                            ver.numRooms += dom.numRooms;
                            ver.numClients += dom.numClients;
                        }
                        dom.version = verName;
                    }

                    nodes[this.id] = this;
                });

                document.title = data.name + " - Merlion Web Console";
                $('#server-name').text(data.name);
                $('#about-server').text(
                    data.master.name + "\n" + data.master.version + "\n" +
                    data.master.copyright + "\n" +
                    data.master.trademark);
                $('#system-info').text(data.master.system);

                nodeTable.update(nodes);
                versionTable.update(versions);
            },
            error: handleAjaxApiError
        });
    }

    function formatCount(count)
    {
        var s = String(count);
        var parts = [];
        var ln = s.length;
        for (var i = 0; i < ln; i += 3) {
            var end = s.length - i;
            var begin = Math.max(end - 3, 0);
            parts.push(s.substr(begin, end - begin));
        }
        for (var a = 0, b = parts.length - 1; a < b; ++a, --b) {
            var p = parts[a]; parts[a] = parts[b];
            parts[b] = p;
        }
        return parts.join(',');
    }

    function formatUpTime(uptime)
    {
        if (uptime < 1) {
            return "< 1s";
        }

        uptime = Math.round(uptime);

        if (uptime < 60) {
            return uptime + "s";
        } else if (uptime < 3600) {
            var s = uptime % 60;
            var m = Math.floor(uptime / 60);
            return m + "m" + s + "s";
        } else if (uptime < 86400) {
            var m = Math.floor(uptime / 60) % 60;
            var h = Math.floor(uptime / 3600);
            return m + "h" + m + "m";
        } else {
            var h = Math.floor(uptime / 3600) % 24;
            var d = Math.floor(uptime / 86400);
            return d + "d" + h + "h";
        }
    }

    var LogView = (function() {
        var currentSeverityFilter = null;
        function Node() {
            this.next = null;
            this.previous = null;
            this.id = null;
            this.e = null;
        }
        Node.prototype = {};
        Node.prototype.insert = function(n) {
            if (n.next !== null || n.previous !== null) {
                throw new Error("Attempted to insert an already inserted element.");
            }
            n.previous = this.previous;
            n.next = this;
            this.previous.next = n;
            this.previous = n;
            if (n.e !== null) {
                if (this.e !== null) {
                    this.e.parentNode.insertBefore(n.e, this.e);
                } else if (n.previous.e !== null) {
                    n.previous.e.parentNode.appendChild(n.e);
                }
            }
        };
        Node.prototype.remove = function() {
            if (this.next === null || this.previous === null) {
                throw new Error("Attempted to remove an already removed element.");
            }

            if (this.e !== null) 
                this.e.parentNode.removeChild(this.e);

            if (this.next == this.previous) {
                this.next.previous = this.next;
                this.next.next = this.next;
            } else {
                this.next.previous = this.previous;
                this.previous.next = this.next;
            }
            this.next = this.previous = null;
        };
        Node.prototype.isLinked = function() {
            return this.next !== null;
        };

        function NotLoadedNode() {
            var self = this;

            this.e = document.createElement('div');
            this.e.className = 'not-loaded';

            var a = this.a = document.createElement('a');
            a.innerHTML = 'Load Items';
            a.onclick = function() {
                self.onClicked();
                return false;
            };
            a.href = '#';
            this.e.appendChild(a);


            this.loading = false;
        }
        NotLoadedNode.prototype = new Node();
        NotLoadedNode.prototype.onClicked = function() {
            if (this.loading) {
                return;
            }

            var self = this;
            var data = {
                severity: currentSeverityFilter
            };

            if (this.next.id !== null) {
                data.before = this.next.id - 1;
            }
            if (this.previous.id !== null) {
                data.since = this.previous.id;
            }
            data.limit = 100;
            if (this.next == this.previous) {
                // Initial view must has a few items
                data.limit = 20;
            }
            this.loading = true;
            this.a.className = 'loading';

            $.ajax({
                url: 'log.json', 
                dataType: 'json',
                data: data,
                type: 'POST',
                success: function(ents) {
                    if (!self.isLinked()) {
                        return;
                    }

                    var p = self.next;

                    for (var i = 0; i < ents.length; ++i) {
                        if (ents[i].id !== data.since)
                            p.insert(new LoadedNode(ents[i]));
                        else
                            self.remove();
                    }

                    if (typeof data.before === 'undefined' && ents.length > 0) {
                        p.insert(new NotLoadedNode());
                    }
                    
                    self.loading = false;
                    self.a.className = '';
                },
                error: function() {
                    handleAjaxApiError(arguments);
                    self.loading = false;
                    self.a.className = '';
                }
            });
        };

        function LoadedNode(ent) {
            this.id = ent.id;
            this.e = document.createElement('div');
            this.e.className = 'item severity-' + ent.severity;

            var $e = $(this.e);

            var time = new Date(ent.timestamp);

            $('<span class="timestamp-local">')
            .text(formatTimestamp(time, true)).appendTo($e);
            $('<span class="timestamp-utc">')
            .text(formatTimestamp(time, false)).appendTo($e);
            $('<span class="host">')
            .text(ent.host).appendTo($e);
            $('<span class="source">')
            .text(ent.source).appendTo($e);
            if (ent.channel !== null && ent.channel !== '' && ent.channel !== '(null)') {
                $('<span class="channel">')
                .text(ent.channel).appendTo($e);
            }
            var msg = $('<span class="msg">')
            .text(ent.msg).appendTo($e);

            if (ent.severity == 'info')
                $('<span class="glyphicon glyphicon-info-sign">').prependTo(msg);
            if (ent.severity == 'warn')
                $('<span class="glyphicon glyphicon-bell">').prependTo(msg);
            if (ent.severity == 'error')
                $('<span class="glyphicon glyphicon-warning-sign">').prependTo(msg);
            if (ent.severity == 'fatal')
                $('<span class="glyphicon glyphicon-ban-circle">').prependTo(msg);

        }
        LoadedNode.prototype = new Node();

        function formatTimestamp(dt, local) {
            var y, m, d, h, m2, s;
            if (local) {
                y = dt.getFullYear();
                m = dt.getMonth() + 1;
                d = dt.getDate();
                h = dt.getHours();
                m2= dt.getMinutes();
                s = dt.getSeconds();
            } else {
                y = dt.getUTCFullYear();
                m = dt.getUTCMonth() + 1;
                d = dt.getUTCDate();
                h = dt.getUTCHours();
                m2= dt.getUTCMinutes();
                s = dt.getUTCSeconds();
            }
            return zeroFill(y, 4) + '/' + m + '/' + d + ' ' +
            h + ':' + zeroFill(m2, 2) + ':' + zeroFill(s, 2);
        }

        function zeroFill(s, nd) {
            s = String(s);
            while (s.length < nd) s = '0' + s;
            return s;
        }

        var root = new Node();
        root.next = root.previous = root;

        // Add first item
        var first = new NotLoadedNode(0);
        $('#log-view')[0].appendChild(first.e);
        root.insert(first);

        function refresh()
        {
            root.previous.onClicked();
        }

        function updateFilter()
        {
            var classes = [];

            if ($('#show-local-time').is(':checked'))
                classes.push('show-local');
            else
                classes.push('show-utc');

            $('#log-view')[0].className = classes.join(' ');
        }

        currentSeverityFilter = 'warn';

        function severityFilterChanged() {
            var lastval = currentSeverityFilter;
            var self = this;
            currentSeverityFilter = null;
            $.each(['debug', 'info', 'warn', 'error', 'fatal'], function() {
                if ($('#log-filter-' + this)[0] == self) {
                    currentSeverityFilter = this;
                }
            });
            if (currentSeverityFilter === null)
                throw new Error("Severity is null");

            if (currentSeverityFilter === lastval)
                return;

            // Remove all items
            while (root.previous !== root)
                root.previous.remove();

            first = new NotLoadedNode(0);
            $('#log-view')[0].appendChild(first.e);
            root.insert(first);
            first.onClicked();
        }

        $.each(['debug', 'info', 'warn', 'error', 'fatal'], function() {
            $('#log-filter-' + this).change(severityFilterChanged);
        });
        $('#show-local-time').change(updateFilter);

        updateFilter();

        return {
            refresh: refresh,
            updateFilter: updateFilter
        };
    })();

    function refreshLog()
    {
        LogView.refresh();
    }

	function refreshAll()
	{
        refreshStatus();
        refreshLog();
	}

    function handleAjaxApiError(xhr, status) {
        var resp = xhr.responseText;
        try {
            var data = JSON.parse(resp);
            if (data['error']) {
                alert("Error occured while performing AJAX operation.\n\n" + 
                    data['errorType'] + ': ' + data['error'] + 
                    '\n\n' + data['detail']);
            } else {
                throw "Unknown error.";
            }
        } catch (e) {
            $('#ajax-error-alert').show();
            try { console.error(e); } catch (e) { }
            //alert("Error occured while performing AJAX operation.\n\n" + status);
        }
    }

	$('#refresh').click(function() {
		refreshAll();
	});

    $('#ajax-error-alert button').click(function() {
        $('#ajax-error-alert').hide();
    }); 

	refreshAll();

	$( "#slider" ).slider({
      value:100,
      min: 0,
      max: 500,
      step: 10,
      range: 'min',
      slide: function( event, ui ) {
        $( "#amount" ).val( "$" + ui.value );
      }
    });
});
