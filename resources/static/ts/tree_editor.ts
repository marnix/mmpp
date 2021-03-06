/// <reference path="jquery.d.ts"/>
/// <reference path="mustache.d.ts"/>

import { assert, get_serial, catch_all } from "./utils";
import { TreeManager, TreeNode } from "./tree";
import { OpQueue } from "./op_queue";

export interface NodePainter {
  paint_node(node : TreeNode) : void;
  set_editor_manager(editor_manager : EditorManager) : void;
}

export class EditorManagerObject {
  visible : boolean = false;
  children_open : boolean = true;
  data2_open : boolean = true;
}

export class EditorManager extends TreeManager {
  parent_div : string;
  op_queue : OpQueue;
  painter : NodePainter;
  reparenting : TreeNode;

  constructor(parent_div : string, painter : NodePainter, op_queue : OpQueue) {
    super();
    this.parent_div = parent_div;
    this.op_queue = op_queue;
    this.painter = painter;
    this.painter.set_editor_manager(this);
    this.reparenting = null;
  }

  creating_node(node : TreeNode, special : any) : Promise< void > {
    let obj = new EditorManagerObject();
    this.set_manager_object(node, obj);
    if (node.is_root()) {
      obj.visible = true;
      let html_code = Mustache.render(STEP_ROOT_TMPL, {
        eid: node.get_tree().get_id(),
        id: node.get_id(),
      });
      $(`#${this.parent_div}`).html(html_code);
    }
    return Promise.resolve();
  }

  destroying_node(node : TreeNode) : Promise< void > {
    let obj : EditorManagerObject = this.get_manager_object(node);
    if (node.is_root()) {
      assert(obj.visible);
      obj.visible = false;
      $(`#${this.parent_div}`).html("");
    }
    return Promise.resolve();
  }

  after_reparenting(parent : TreeNode, child : TreeNode, idx : number) : void {
    let parent_obj : EditorManagerObject = this.get_manager_object(parent);
    let child_obj : EditorManagerObject = this.get_manager_object(child);
    assert(!child_obj.visible);
    if (parent_obj.visible) {
      this.make_subtree_visible(parent, child, idx);
    }
  }

  before_orphaning(parent : TreeNode, child : TreeNode, idx : number) : void {
    let parent_obj : EditorManagerObject = this.get_manager_object(parent);
    let child_obj : EditorManagerObject = this.get_manager_object(child);
    assert(parent_obj.visible === child_obj.visible);
    if (child_obj.visible) {
      this.make_subtree_hidden(child);
    }
    let full_id = this.compute_full_id(child);
    $(`#${full_id}`).remove();
  }

  compute_full_id(node : TreeNode) : string {
    return `${node.get_tree().get_id()}_step_${node.get_id()}`;
  }

  compute_data1_element(node : TreeNode) : JQuery {
    return $(`#${this.compute_full_id(node)}_data1`);
  }

  compute_data2_element(node : TreeNode) : JQuery {
    return $(`#${this.compute_full_id(node)}_data2`);
  }

  toggle_children(node : TreeNode, animation : boolean = true) : void {
    let obj : EditorManagerObject = this.get_manager_object(node);
    let full_id = this.compute_full_id(node);
    if (obj.children_open) {
      $(`#${full_id}_btn_toggle_children`).removeClass("mini_button_open").addClass("mini_button_closed");
      if (animation) {
        $(`#${full_id}_children`).slideUp();
      } else {
        $(`#${full_id}_children`).hide();
      }
      obj.children_open = false;
    } else {
      $(`#${full_id}_btn_toggle_children`).removeClass("mini_button_closed").addClass("mini_button_open");
      if (animation) {
        $(`#${full_id}_children`).slideDown();
      } else {
        $(`#${full_id}_children`).show();
      }
      obj.children_open = true;
    }
  }

  open_children(node : TreeNode, animation : boolean = true) : void {
    let obj : EditorManagerObject = this.get_manager_object(node);
    if (!obj.children_open) {
      this.toggle_children(node, animation);
    }
  }

  close_children(node : TreeNode, animation : boolean = true) : void {
    let obj : EditorManagerObject = this.get_manager_object(node);
    if (obj.children_open) {
      this.toggle_children(node, animation);
    }
  }

  toggle_data2(node : TreeNode, animation : boolean = true) : void {
    let obj : EditorManagerObject = this.get_manager_object(node);
    let full_id = this.compute_full_id(node);
    if (obj.data2_open) {
      $(`#${full_id}_btn_toggle_data2`).removeClass("mini_button_open").addClass("mini_button_closed");
      if (animation) {
        $(`#${full_id}_data2`).slideUp();
      } else {
        $(`#${full_id}_data2`).hide();
      }
      obj.data2_open = false;
    } else {
      $(`#${full_id}_btn_toggle_data2`).removeClass("mini_button_closed").addClass("mini_button_open");
      if (animation) {
        $(`#${full_id}_data2`).slideDown();
      } else {
        $(`#${full_id}_data2`).show();
      }
      obj.data2_open = true;
    }
  }

  open_data2(node : TreeNode, animation : boolean = true) : void {
    let obj : EditorManagerObject = this.get_manager_object(node);
    if (!obj.data2_open) {
      this.toggle_data2(node, animation);
    }
  }

  close_data2(node : TreeNode, animation : boolean = true) : void {
    let obj : EditorManagerObject = this.get_manager_object(node);
    if (obj.data2_open) {
      this.toggle_data2(node, animation);
    }
  }

  close_all_children(node : TreeNode) : void {
    this.open_children(node);
    for (let child of node.get_children()) {
      this.close_children(child);
    }
  }

  create_child(node : TreeNode) : void {
    this.reparent(null);
    let tree = node.get_tree();
    tree.create_node(get_serial(), false).then(function (node2 : TreeNode) : void {
      node2.reparent(node, -1);
    }).catch(catch_all);
  }

  kill_node(node : TreeNode) : void {
    this.reparent(null);
    let tree = node.get_tree();
    /* In order to avoid leftovers and not violate the protocol, I need to orphan
    all the descendants (including the base node), and then destroy them. */
    // We make a copy of the array of children, to avoid iterating on it while removing elements
    let children_copy = node.get_children().slice();
    for (let child of children_copy) {
      this.kill_node(child);
    }
    node.orphan();
    tree.destroy_node(node).catch(catch_all);
  }

  move(node : TreeNode, up : boolean) : void {
    let self = this;
    let parent = node.get_parent();
    let idx = parent.find_child_idx(node);
    let parent_children_num = parent.get_children().length;
    let new_idx = idx + (up ? -1 : 1);
    if (0 <= new_idx && new_idx < parent_children_num) {
      this.op_queue.enqueue_operation(function() : Promise< void > {
        node.orphan();
        self.op_queue.enqueue_operation(function() : Promise< void > {
          node.reparent(parent, new_idx);
          return Promise.resolve();
        });
        return Promise.resolve();
      });
    }
  }

  move_up(node : TreeNode) : void {
    this.reparent(null);
    this.move(node, true);
  }

  move_down(node : TreeNode) : void {
    this.reparent(null);
    this.move(node, false);
  }

  reparent(node : TreeNode) : void {
    let self = this;
    let reparenting  = this.reparenting;
    let reparenting_to = node;
    if (this.reparenting === null && node !== null) {
      this.reparenting = node;
      $(`.mini_button_reparent`).addClass("mini_button_reparent_here");
    } else {
      if (node !== null && !reparenting.destroyed && !reparenting_to.destroyed) {
        // Check that reparenting_to is not a descendant of reparenting
        if (!reparenting_to.is_descendant(reparenting)) {
          this.op_queue.enqueue_operation(function() : Promise< void > {
            reparenting.orphan();
            self.op_queue.enqueue_operation(function () : Promise< void > {
              reparenting.reparent(reparenting_to, -1);
              return Promise.resolve();
            });
            return Promise.resolve();
          });
        }
      }
      $(`.mini_button_reparent`).removeClass("mini_button_reparent_here");
      this.reparenting = null;
    }
  }

  make_subtree_visible(parent : TreeNode, child : TreeNode, idx : number) : void {
    let self = this;
    let parent_obj : EditorManagerObject = this.get_manager_object(parent);
    let child_obj : EditorManagerObject = this.get_manager_object(child);
    assert(!child_obj.visible);
    assert(parent_obj.visible);

    child_obj.visible = true;

    let tree_id = child.get_tree().get_id();
    let child_id = child.get_id();
    let child_full_id = this.compute_full_id(child);
    let parent_full_id = this.compute_full_id(parent);

    // Add new code to DOM
    let html_code = Mustache.render(STEP_TMPL, {
      eid: tree_id,
      id: child.get_id(),
    });
    if (idx === 0) {
      $(`#${parent_full_id}_children`).prepend(html_code);
    } else {
      let prev_child_full_id = this.compute_full_id(parent.get_child(idx-1));
      $(`#${prev_child_full_id}`).after(html_code);
    }
    if (child_obj.children_open) {
      this.close_children(child, false);
      this.open_children(child, false);
    } else {
      this.open_children(child, false);
      this.close_children(child, false);
    }
    $(`#${child_full_id}_btn_toggle_children`).click(this.toggle_children.bind(this, child));
    if (child_obj.data2_open) {
      this.close_data2(child, false);
      this.open_data2(child, false);
    } else {
      this.open_data2(child, false);
      this.close_data2(child, false);
    }
    $(`#${child_full_id}_btn_toggle_data2`).click(this.toggle_data2.bind(this, child));
    $(`#${child_full_id}_btn_close_all_children`).click(this.close_all_children.bind(this, child));
    $(`#${child_full_id}_btn_create`).click(this.create_child.bind(this, child));
    $(`#${child_full_id}_btn_kill`).click(this.kill_node.bind(this, child));
    $(`#${child_full_id}_btn_move_up`).click(this.move_up.bind(this, child));
    $(`#${child_full_id}_btn_move_down`).click(this.move_down.bind(this, child));
    $(`#${child_full_id}_btn_reparent`).click(this.reparent.bind(this, child));
    this.painter.paint_node(child);

    // Recur on children
    for (let [idx2, child2] of child.get_children().entries()) {
      this.make_subtree_visible(child, child2, idx2);
    }
  }

  make_subtree_hidden(node : TreeNode) : void {
    let obj : EditorManagerObject = this.get_manager_object(node);
    assert(obj.visible);
    obj.visible = false;
    for (let child of node.children) {
      this.make_subtree_hidden(child);
    }
  }
}

const STEP_ROOT_TMPL = `
<div id="{{ eid }}_step_{{ id }}" class="step">
  <div id="{{ eid }}_step_{{ id }}_children"></div>
</div>
`;

const STEP_TMPL = `
<div id="{{ eid }}_step_{{ id }}" class="step">
  <div id="{{ eid }}_step_{{ id }}_row" class="step_row">
    <div id="{{ eid }}_step_{{ id }}_handle" class="step_handle">
      <button id="{{ eid }}_step_{{ id }}_btn_toggle_children" class="mini_button"></button>
      <button id="{{ eid }}_step_{{ id }}_btn_toggle_data2" class="mini_button"></button>
      <button id="{{ eid }}_step_{{ id }}_btn_close_all_children" class="mini_button mini_button_close_all_children"></button>
      <button id="{{ eid }}_step_{{ id }}_btn_create" class="mini_button mini_button_create"></button>
      <button id="{{ eid }}_step_{{ id }}_btn_kill" class="mini_button mini_button_kill"></button>
      <button id="{{ eid }}_step_{{ id }}_btn_move_up" class="mini_button mini_button_move_up"></button>
      <button id="{{ eid }}_step_{{ id }}_btn_move_down" class="mini_button mini_button_move_down"></button>
      <button id="{{ eid }}_step_{{ id }}_btn_reparent" class="mini_button mini_button_reparent"></button>
    </div>
    <div id="{{ eid }}_step_{{ id }}_data" class="step_data">
      <div id="{{ eid }}_step_{{ id }}_data1" class="step_data1"></div>
      <div id="{{ eid }}_step_{{ id }}_data2" class="step_data2" style="display: none;"></div>
    </div>
  </div>
  <div id="{{ eid }}_step_{{ id }}_children" class="step_children"></div>
</div>
`;
