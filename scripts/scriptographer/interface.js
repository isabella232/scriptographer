importPackage(Packages.com.scriptographer);

var baseDir = ScriptographerEngine.baseDir;
var scriptoDir = new File(baseDir, "startup/scriptographer");

var lineHeight = 16;
var scriptImage = new Image(scriptoDir + "/script.png");
var folderImage = new Image(scriptoDir + "/folder.png");
var playImage = new Image(scriptoDir + "/play.png");
var stopImage = new Image(scriptoDir + "/stop.png");
var refreshImage = new Image(scriptoDir + "/refresh.png");
var consoleImage = new Image(scriptoDir + "/console.png");
var tool1Image = new Image(scriptoDir + "/tool1.png");
var tool2Image = new Image(scriptoDir + "/tool2.png");
var buttonRect = new Rectangle(0, 0, 32, 17);

var consoleDialog = new Dialog(Dialog.STYLE_TABBED_RESIZING_FLOATING, "Scriptographer Console", new Rectangle(200, 200, 400, 300), Dialog.OPTION_TABBED_DIALOG_SHOWS_CYCLE, function() {
	var textIn = new TextEdit(this, new Rectangle(0, 0, 300, 50), "", TextEdit.STYLE_MULTILINE);
	var textOut = new TextEdit(this, new Rectangle(0, 0, 300, 100), "", TextEdit.STYLE_READONLY | TextEdit.STYLE_MULTILINE);
	textIn.setMinimumSize(200, 18);
	textOut.setMinimumSize(200, 18);
	textOut.setBackgroundColor(Drawer.COLOR_INACTIVE_TAB);

	var newLine = java.lang.System.getProperty("line.separator");
	
	// out:
	
	var consoleText = new java.lang.StringBuffer();
	
	consoleWriter = new ConsoleOutputWriter() {
		println: function(str) {
			if (textOut) {
				// if the text does not grow too long, remove old lines again:
				consoleText.append(str);
				consoleText.append(newLine);
				while (consoleText.length() >= 32768) {
					var pos = consoleText.indexOf(newLine);
					if (pos == -1) pos = consoleText.length() - 1;
					consoleText["delete"](0, pos + 1);
				}
				textOut.text = consoleText.toString();
				var end = consoleText.length();
				textOut.selection = [end, end];
				consoleDialog.visible = true;
			}
		}
	};
	
	ConsoleOutputStream.getInstance().setWriter(consoleWriter);

	this.onDestroy = function() {
		textOut = null;
	}
	
	// in:
	
	var consoleScope = {}

	textIn.onTrack = function(tracker) {
		if (tracker.action == Tracker.ACTION_KEY_STROKE && tracker.virtualKey == Tracker.KEY_RETURN) {
			// enter was pressed in the input field. determine the current line:
			var text = this.text;
			var ch;
			var end = this.selection[1] - 1;
			while (end >= 0 && ((ch = text[end]) == '\n' || ch == '\r'))
				end--;
			var start = end;
			end++;
			while (start >= 0 && ((ch = text[start]) != '\n' && ch != '\r'))
				start--;
			start++;
			text = text.substring(start, end);
			// use the global evaluate (defined by scriptographer) as the console's function
			// so it's executed in its own scope:
			consoleScope.evaluate = evaluate;
			// remove the function before the actual code is evaluated, otherwise it may be in the way there:
			consoleScope.evaluate("delete evaluate;" + newLine + text);
		}
	}

	// buttons:

	var clearButton = new PushButton(this, buttonRect, refreshImage);
	clearButton.onChange = function() {
		textOut.text = "";
		consoleText.setLength(0);
	}
	
	// layout:
	this.setInsets(-1, -1, -1, -1);
	this.setLayout(new TableLayout([ TableLayout.FILL ], [ 0.2, TableLayout.FILL, 15 ], -1 , -1));
	this.addToLayout(textIn, "0, 0");
	this.addToLayout(textOut, "0, 1");
	
	var buttons = new ItemContainer(new FlowLayout(FlowLayout.LEFT, -1, -1));
	buttons.add(clearButton);
	this.addToLayout(buttons, "0, 2");
});

var mainDialog = new Dialog(Dialog.STYLE_TABBED_RESIZING_FLOATING, "Scriptographer", new Rectangle(100, 100, 400, 400), Dialog.OPTION_TABBED_DIALOG_SHOWS_CYCLE, function() {
	// Script List:
	var scriptListBox = new HierarchyListBox(this, new Rectangle(0, 0, 241, 20 * lineHeight), HierarchyListBox.STYLE_BLACK_RECT);

	var scriptList = scriptListBox.getList();
	with (scriptList) {
//		setBackgroundColor(Drawer.COLOR_BACKGROUND);
		setEntrySize(2000, lineHeight);
		setEntryTextRect(0, 0, 2000, lineHeight);
		
		onTrack = function(tracker, entry) {
			if (tracker.action == Tracker.ACTION_BUTTON_UP && (tracker.modifiers & Tracker.MODIFIER_DOUBLE_CLICK)) {
				if (entry.isDirectory) {
					entry.expanded = !entry.expanded;
					entry.list.invalidate();
				} else {
					execute(entry.file);
				}
			}
		}
	}

	// filter for hiding files:
	filter = new java.io.FilenameFilter() {
		accept: function(dir, name) {
			name = new java.lang.String(name);
			return name != "CVS" && !name.startsWith(".") &&
					(name.endsWith(".js") || new File(dir, name).isDirectory());
		}
	};

	
	var addFiles = function(list, dir) {
		if (!list) list = scriptList;
		if (!dir) dir = baseDir;
		var files = dir.listFiles(filter);
		for (var i in files) {
			var file = files[i];
			var entry = new HierarchyListEntry(list);
			entry.text = file.name;
			entry.file = file;
			entry.isDirectory = file.isDirectory();
//			entry.setBackgroundColor(Drawer.COLOR_BACKGROUND);
			if (entry.isDirectory) {
				addFiles(entry.createChildList(), file);
				entry.expanded = false;
				entry.picture = folderImage;
			} else {
				entry.picture = scriptImage;
			}
		}
	}
	
	var removeFiles = function() {
		for (var i = scriptList.getNumEntries() - 1; i >= 0; i--)
			scriptList.removeEntry(i);
	}

	addFiles();

	// buttons:
	
	var playButton = new PushButton(this, buttonRect, playImage);
	playButton.onChange = function() {
		var entry = scriptList.getActiveEntry();
		if (entry && entry.file) {
			execute(entry.file);
		}
	}

	var stopButton = new PushButton(this, buttonRect, stopImage);
	stopButton.onChange = function() {
		print('stop');
	}

	var refreshButton = new PushButton(this, buttonRect, refreshImage);
	refreshButton.onChange = function() {
		removeFiles();
		addFiles();
	}

	var consoleButton = new PushButton(this, buttonRect, consoleImage);
	consoleButton.onChange = function() {
		consoleDialog.visible = !consoleDialog.visible;
	}

	var newButton = new PushButton(this, buttonRect, scriptImage);
	newButton.onChange = function() {
		print('new');
	}

	var tool1Button = new PushButton(this, buttonRect, tool1Image);
	tool1Button.toolIndex = 0;
	tool1Button.entryPicture = tool1Image;

	var tool2Button = new PushButton(this, buttonRect, tool2Image);
	tool2Button.toolIndex = 1;
	tool2Button.entryPicture = tool2Image;
	
	tool1Button.onChange = tool2Button.onChange = function() {
		var entry = scriptList.getActiveEntry();
		if (entry && entry.file) {
			Tool.getTool(this.toolIndex).setScript(entry.file);
			if (entry != this.curEntry) {
				if (this.curEntry)
					this.curEntry.picture = scriptImage;
				entry.picture = this.entryPicture;
				this.curEntry = entry;
			}
		}
	}

	// layout:
	this.setInsets(-1, 0, -1, -1);
	this.setLayout(new BorderLayout());
	
	this.addToLayout(scriptListBox, BorderLayout.CENTER);
	
	var buttons = new ItemContainer(new FlowLayout(FlowLayout.LEFT, -1, -1));
	buttons.add(playButton);
	buttons.add(stopButton);
	buttons.add(new Spacer(4, 0));
	buttons.add(refreshButton);
	buttons.add(new Spacer(4, 0));
	buttons.add(newButton);
	buttons.add(consoleButton);
	buttons.add(new Spacer(4, 0));
	buttons.add(tool1Button);
	buttons.add(tool2Button);
	
	this.addToLayout(buttons, BorderLayout.SOUTH);
});

mainDialog.onClose = function() {
	this.destroy();
	consoleDialog.destroy();
}

mainDialog.onResize = function(dx, dy) {
//	print(dx, dy);
}