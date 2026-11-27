import { useReducer } from 'react';
import type { VfxProject } from '../shared/types.js';
import { createProject } from '../shared/templates.js';
interface EditorState { project: VfxProject; past: VfxProject[]; future: VfxProject[]; }
type Action = { type: 'edit'; edit: (draft: VfxProject) => void } | { type: 'replace'; project: VfxProject } | { type: 'undo' | 'redo' };
export function fingerprint(project: VfxProject): string { return JSON.stringify({ ...project, revision: 0 }); }
function reducer(state: EditorState, action: Action): EditorState {
  if (action.type === 'replace') return { project: action.project, past: [], future: [] };
  if (action.type === 'edit') {
    const next = structuredClone(state.project); action.edit(next);
    if (fingerprint(next) === fingerprint(state.project)) return state;
    next.revision = state.project.revision + 1;
    return { project: next, past: [...state.past, state.project].slice(-100), future: [] };
  }
  if (action.type === 'undo' && state.past.length) {
    const previous = state.past[state.past.length - 1];
    return { project: { ...previous, revision: state.project.revision + 1 }, past: state.past.slice(0, -1), future: [state.project, ...state.future] };
  }
  if (action.type === 'redo' && state.future.length) return { project: { ...state.future[0], revision: state.project.revision + 1 }, past: [...state.past, state.project].slice(-100), future: state.future.slice(1) };
  return state;
}
export function useEditor() {
  const [state, dispatch] = useReducer(reducer, undefined, () => ({ project: createProject('magic'), past: [], future: [] }));
  return { ...state, edit: (edit: (draft: VfxProject) => void) => dispatch({ type: 'edit', edit }),
    replace: (project: VfxProject) => dispatch({ type: 'replace', project }), undo: () => dispatch({ type: 'undo' }), redo: () => dispatch({ type: 'redo' }) };
}
