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
                        rows[key].element = options.create(rows[key], data[key]).appendTo(parent)
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

    var domainTable = bindList($('#domain-table'), {
        create: function (row, data) {
            var tr = row.tr = $('<tr>');
            row.dNode = $('<td>').appendTo(tr);
            row.dVer = $('<td>').appendTo(tr);
            row.dNumRooms = $('<td>').appendTo(tr);
            row.dNumClients = $('<td>').appendTo(tr);
            row.dUpTime = $('<td>').appendTo(tr);
            row.dActions = $('<td>').appendTo(tr);
            row.dUpTime.text('--');
            row.dActions.text('--');
            return this.update(row, data);
        },
        update: function (row, data) {
            row.dNode.text(data.node);
            row.dVer.text(data.version);
            row.dNumRooms.text(data.numRooms);
            row.dNumClients.text(data.numClients);
            return row.tr;
        }
    });

    var versionTable = bindList($('#version-table'), {
        create: function (row, data) {
            var tr = row.tr = $('<tr>');
            row.dVer = $('<td>').appendTo(tr);
            row.dNumRooms = $('<td>').appendTo(tr);
            row.dNumClients = $('<td>').appendTo(tr);
            row.dThrottle = $('<td>').appendTo(tr);
            row.dActions = $('<td>').appendTo(tr);

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
            row.dNumRooms.text(data.numRooms);
            row.dNumClients.text(data.numClients);
            row.lastValue = data.throttle;
            row.dThrottleSlider.slider('value', data.throttle);
            return row.tr;
        }
    });

    var nodeTable = bindList($('#server-table'), {
        create: function (row, data) {
            var tr = row.tr = $('<tr>');
            row.dNode = $('<td>').appendTo(tr);
            row.dHost = $('<td>').appendTo(tr);
            row.dNumRooms = $('<td>').appendTo(tr);
            row.dNumClients = $('<td>').appendTo(tr);
            row.dUpTime = $('<td>').appendTo(tr);
            row.dThrottle = $('<td>').appendTo(tr);
            row.dActions = $('<td>').appendTo(tr);

            row.dThrottleSlider = $('<div>').appendTo(row.dThrottle)
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
            $('<button class="btn btn-danger btn-sm">')
            .text('Remove').appendTo(row.dActions)
            .click(function() {
                $.ajax({
                    url: '/node/destroy.json',
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
            row.dNode.text(data.name);
            row.dHost.text(data.host);
            row.dNumRooms.text(data.numRooms);
            row.dNumClients.text(data.numClients);
            row.dUpTime.text(formatUpTime(data.uptime));
            row.lastValue = data.throttle;
            row.dThrottleSlider.slider('value', data.throttle);
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

                        domains[this.name + '/' + verName] = {
                            node: this.name,
                            version: verName,
                            numRooms: dom.numRooms,
                            numClients: dom.numClients
                        };
                    }

                    nodes[this.id] = this;
                });



                domainTable.update(domains);
                nodeTable.update(nodes);
                versionTable.update(versions);
            },
            error: function() {
                alert("AJAX error.");
            }
        });
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
                this.next.previous = null;
                this.next.next = null;
            } else {
                this.next.previous = this.previous;
                this.previous.next = this.next;
            }
            this.next = this.previous = null;
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
            var data = {};

            if (this.next.id !== null) {
                data.before = this.next.id - 1;
            }
            if (this.previous.id !== null) {
                data.since = this.previous.id;
            }
            data.limit = 100;
            this.loading = true;
            this.a.className = 'loading';

            $.ajax({
                url: 'log.json', 
                dataType: 'json',
                data: data,
                type: 'POST',
                success: function(ents) {
                    var p = self.next;

                    for (var i = 0; i < ents.length; ++i) {
                        if (ents[i].id !== data.since)
                            p.insert(new LoadedNode(ents[i]));
                        else
                            self.remove();
                    }

                    if (typeof data.before === 'undefined') {
                        p.insert(new NotLoadedNode());
                    }
                    
                    self.loading = false;
                    self.a.className = '';
                },
                error: function() {
                    alert("AJAX error.");
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
            $.each(['debug', 'info', 'warn', 'error', 'fatal'], function() {
                if ($('#log-filter-' + this).is(':checked')) {
                    classes.push('show-' + this);
                }
            });

            if ($('#show-local-time').is(':checked'))
                classes.push('show-local');
            else
                classes.push('show-utc');

            $('#log-view')[0].className = classes.join(' ');
        }

        $.each(['debug', 'info', 'warn', 'error', 'fatal'], function() {
            $('#log-filter-' + this).change(updateFilter);
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
            alert("Error occured while performing AJAX operation.\n\n" + status);
        }
    }

	$('#refresh').click(function() {
		refreshAll();
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
