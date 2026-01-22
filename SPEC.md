# Compose — Email Editor Technical Specification

> A single-page markdown email editor with file attachments, session management, and ZIP import/export.

## Overview

**Purpose**: A browser-based email composition tool that allows users to write content in Markdown with live preview, attach files/images, organize work into sessions, and export/import sessions as ZIP archives.

**Key Constraints**:
- Single HTML file (no build step)
- Must handle large files (500MB+) without freezing
- Offline-capable (all data stored locally)
- ZIP exports must work when opened outside the app

---

## Tech Stack

| Layer | Technology |
|-------|------------|
| UI Framework | React 18 (via CDN, Babel standalone) |
| Markdown | marked.js |
| ZIP Handling | JSZip + FileSaver.js |
| Styling | Vanilla CSS (CSS variables, no framework) |
| Storage | localStorage (metadata) + IndexedDB (file blobs) |

### CDN Dependencies

```html
<script src="https://cdnjs.cloudflare.com/ajax/libs/react/18.2.0/umd/react.production.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/react-dom/18.2.0/umd/react-dom.production.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/babel-standalone/7.23.5/babel.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/marked/9.1.6/marked.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/jszip/3.10.1/jszip.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/FileSaver.js/2.0.5/FileSaver.min.js"></script>
```

---

## Data Architecture

### Session Model

```typescript
interface Session {
  id: string;           // Random alphanumeric (9 chars)
  name: string;         // Derived from first line of content (max 40 chars)
  content: string;      // Markdown text with attachment:id references
  files: FileMetadata[];
  createdAt: number;    // Unix timestamp
  updatedAt: number;    // Unix timestamp
}

interface FileMetadata {
  id: string;           // Random alphanumeric (9 chars)
  name: string;         // Original filename
  type: string;         // MIME type
  size: number;         // Bytes
  objectUrl?: string;   // Temporary blob URL for preview (not persisted)
}
```

### Storage Strategy

| Data | Storage | Reason |
|------|---------|--------|
| Session metadata | `localStorage` | Small, JSON-serializable, survives refresh |
| File blobs | `IndexedDB` | Handles large binary data efficiently |
| Preview URLs | `URL.createObjectURL()` | Zero memory overhead, auto-revoked |

#### IndexedDB Schema

```
Database: ComposeFilesDB
└── Object Store: files
    └── keyPath: id
    └── Value: { id: string, blob: Blob }
```

---

## Core Features

### 1. Markdown Editor

- **Split view modes**: Edit | Split | Preview
- **Live rendering**: Content parsed via `marked.parse()` on every change
- **Session name derivation**: First heading or first 40 chars of content

### 2. Inline Image Pasting (⌘V / Ctrl+V)

**Flow**:
1. Intercept `paste` event on textarea
2. Check `clipboardData.items` for image types
3. Prevent default, get cursor position
4. Call `handleFilesAdd(files, insertInline=true, cursorPos)`
5. Insert `![filename](attachment:id)` at cursor
6. Store blob in IndexedDB, create object URL for preview

**Markdown syntax**:
```markdown
![image_1737012345.png](attachment:abc123def)
```

### 3. File Attachments

**Supported input methods**:
- Drag & drop (anywhere on page)
- File picker button
- Clipboard paste (images)

**Processing flow**:
```
User drops file
    ↓
Create object URL (instant preview)
    ↓
Store blob in IndexedDB (async, non-blocking)
    ↓
Update session.files[] with metadata
    ↓
If image + inline mode: insert markdown reference
```

### 4. Image Preview Resolution

In the preview pane, `attachment:id` references are resolved to object URLs:

```javascript
activeSession.files.forEach(file => {
  content = content.replace(
    new RegExp(`attachment:${file.id}`, 'g'),
    file.objectUrl || objectUrlsRef.current.get(file.id)
  );
});
```

---

## ZIP Export/Import

### Export Structure

```
session_name.zip
├── content.md          # Markdown with relative paths
├── metadata.json       # Session metadata
└── attachments/
    ├── image1.png
    ├── document.pdf
    └── ...
```

### Export Process

1. Clone content, replace `attachment:id` → `./attachments/filename`
2. Fetch blobs from IndexedDB
3. Add blobs directly to JSZip (no base64 conversion!)
4. Generate with progress callback
5. Trigger download via FileSaver.js

**Performance optimizations**:
- Pass `Blob` directly to JSZip (not base64)
- Use `STORE` compression for files >100MB
- Lower compression level (3) for speed
- Progress callback for UI feedback

```javascript
zip.generateAsync({ 
  type: 'blob',
  compression: totalSize > 100MB ? 'STORE' : 'DEFLATE',
  compressionOptions: { level: 3 }
}, (metadata) => {
  updateProgress(metadata.percent);
});
```

### Import Process

1. Load ZIP via `JSZip.loadAsync(file)`
2. Read `content.md` and `metadata.json`
3. For each file in `attachments/`:
   - Get as blob: `zipEntry.async('blob')`
   - Generate new ID
   - Store in IndexedDB
   - Create object URL
   - Build filename → newId mapping
4. Replace `./attachments/filename` → `attachment:newId` in content
5. Create new session

---

## Performance Considerations

### Problem: Large files freeze the UI

**Solutions implemented**:

| Technique | Purpose |
|-----------|---------|
| `URL.createObjectURL()` | Instant preview without reading file into memory |
| IndexedDB for blobs | Efficient binary storage, doesn't block main thread |
| Direct blob → JSZip | Avoids expensive base64 encoding |
| Adaptive compression | Skip compression for very large files |
| Progress indicators | User feedback during long operations |
| `isProcessing` state | Prevent concurrent operations |

### Memory Management

```javascript
// Track object URLs for cleanup
const objectUrlsRef = useRef(new Map());

// Cleanup on unmount
useEffect(() => {
  return () => {
    objectUrlsRef.current.forEach(url => URL.revokeObjectURL(url));
  };
}, []);

// Cleanup on file removal
const removeFile = async (fileId) => {
  URL.revokeObjectURL(objectUrlsRef.current.get(fileId));
  objectUrlsRef.current.delete(fileId);
  await FileDB.deleteFile(fileId);
};
```

---

## UI Components

### Layout

```
┌─────────────────────────────────────────────────────────┐
│ Sidebar (280px)        │ Main Content                   │
│ ┌───────────────────┐  │ ┌───────────────────────────┐  │
│ │ Logo              │  │ │ Toolbar                   │  │
│ ├───────────────────┤  │ │ [Edit][Split][Preview]    │  │
│ │ Sessions Header   │  │ │ [Add Files] [Import ZIP]  │  │
│ │ [+ New Session]   │  │ ├───────────────────────────┤  │
│ ├───────────────────┤  │ │ Editor    │ Preview       │  │
│ │ Session List      │  │ │           │               │  │
│ │ - Session 1 ✓     │  │ │ Markdown  │ Rendered      │  │
│ │ - Session 2       │  │ │ textarea  │ HTML          │  │
│ │ - Session 3       │  │ │           │               │  │
│ │                   │  │ ├───────────────────────────┤  │
│ │                   │  │ │ Attachments (collapsible) │  │
│ │                   │  │ │ [file] [file] [file]      │  │
│ └───────────────────┘  │ └───────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### Component Tree

```
App
├── Sidebar
│   ├── Logo
│   ├── SessionsHeader (+ new button)
│   └── SessionsList
│       └── SessionItem (name, date, file count, actions)
├── Main
│   ├── Toolbar (view toggle, file buttons)
│   ├── EditorContainer
│   │   ├── EditorPane (textarea)
│   │   └── PreviewPane (rendered HTML)
│   └── FilesSection
│       ├── FilesHeader (collapsible)
│       └── FilesGrid
│           └── FileCard (preview, name, size, remove)
├── DropZone (overlay when dragging)
└── Toast (notifications)
```

---

## State Management

```javascript
const [sessions, setSessions] = useState([...]);      // All sessions
const [activeSessionId, setActiveSessionId] = useState('...');
const [viewMode, setViewMode] = useState('split');    // 'edit' | 'split' | 'preview'
const [showFiles, setShowFiles] = useState(true);     // Attachments panel
const [isDragging, setIsDragging] = useState(false);  // Drop zone visibility
const [isProcessing, setIsProcessing] = useState(false); // Block concurrent ops
const [toast, setToast] = useState({...});            // Notifications

const objectUrlsRef = useRef(new Map());              // Track blob URLs
```

---

## Design System

### Colors (Dark Theme)

```css
--bg-primary: #0a0a0b;
--bg-secondary: #111113;
--bg-tertiary: #1a1a1d;
--bg-hover: #222225;
--border: #2a2a2e;
--border-active: #3a3a3f;
--text-primary: #fafafa;
--text-secondary: #a0a0a5;
--text-muted: #606065;
--accent: #ff6b35;          /* Orange */
--accent-soft: rgba(255, 107, 53, 0.15);
--success: #10b981;
--danger: #ef4444;
```

### Typography

```css
font-family: 'Outfit', -apple-system, sans-serif;     /* UI */
font-family: 'JetBrains Mono', monospace;             /* Editor */
```

---

## Key Functions Reference

| Function | Purpose |
|----------|---------|
| `handleFilesAdd(files, insertInline?, cursorPos?)` | Process dropped/pasted/selected files |
| `handlePaste(e)` | Intercept clipboard for images |
| `exportAsZip(session)` | Generate and download ZIP |
| `importFromZip(file)` | Parse ZIP and create session |
| `FileDB.saveFile(id, blob)` | Store blob in IndexedDB |
| `FileDB.getFile(id)` | Retrieve blob from IndexedDB |
| `getFilePreviewUrl(file)` | Get or create object URL |
| `removeFile(fileId)` | Delete file + cleanup URLs |

---

## Future Enhancements

- [ ] Rich text formatting toolbar
- [ ] Image resize/crop before insert
- [ ] Cloud sync (optional)
- [ ] Collaborative editing
- [ ] Email sending integration
- [ ] Template library
- [ ] Search across sessions
- [ ] Keyboard shortcuts
- [ ] Export to HTML/PDF
- [ ] Dark/light theme toggle
