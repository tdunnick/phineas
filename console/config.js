/*
 * config.js
 *
 * Copyright 2011 Thomas L Dunnick
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * stuff needed to support our configuration form and processing
 */

// find the ancestor matching this tag

function findAncestor(o, tag)
{
  while (o && (o.nodeName != tag))
  {
    if (o.parentNode) o = o.parentNode;
    else if (o.offsetParent) o = o.offsetParent;
  }
  return (o);
}

// find the previous sibling matching this tag

function findPrevSibling(o, tag)
{
  while (o && (o.nodeName != tag))
  o = o.previousSibling;
  return (o);
}

// find the next sibling matching this tag

function findNextSibling(o, tag)
{
  while (o && (o.nodeName != tag))
  o = o.nextSibling;
  return (o);
}

// find the child matching this tag

function findChild(o, tag)
{
  if (o) o = findNextSibling(o.firstChild, tag);
  return (o);
}

// add a repeated field - assumes at least one parent TR with a 
// prompt and input in each of two TD elements.

function addfield(o)
{
  var fname = o.name;
  o = findAncestor(o, "TR");
  // get the index of the previous row
  var p = findPrevSibling(o.previousSibling, "TR");
  p = findChild(p, "TD");
  var i = p.innerHTML.replace(new RegExp("^\\s*" + fname), "") - 0;
  // create a new table row
  var c = document.createElement("TR");
  var d = document.createElement("TD");
  // add the prompt
  d.appendChild(document.createTextNode(fname + " " + (i + 1)));
  c.appendChild(d);
  // get input entry
  p = findChild(findNextSibling(p.nextSibling, "TD"), "INPUT");
  var f = document.createElement("INPUT");
  f.type = p.type;
  f.size = p.size;
  f.name = p.name.replace(new RegExp(fname + ".*\\]"), fname + "[" + i + "]");
  d = document.createElement("TD");
  d.appendChild(f);
  c.appendChild(d);
  // finally place it in the form
  if (o.parentNode) o.parentNode.insertBefore(c, o);
  else o.offsetParent.insertBefore(c, o);
}

/*
 * our submit function...  
 * set the hidden request value to a name value pair to get around 
 * differences in how browser pass button values and to suppress
 * accidental submission by the Enter key and make user confirm
 * the submission.
 */

function setRequest(o, v)
{
  o.form.ConfigurationRequest.value = o.name + ":" + v;
  if (confirm(o.value)) o.form.submit();
}

/*
 * Support for bubble type help tips.
 * The following is loosely based on Bubble Tooltips by Alessandro Fulciniti
 * - http://pro.html.it - http://web-graphics.com 
 *
 * provide ballon tips help when mouse enters element area
 */

function showHelp (o, h)
{
  var b = createHelp (h);
  if (b != null)
    moveHelp (o, b);
}

/*
 * hide the help bubble after mouse leaves element area
 */
function hideHelp ()
{
  if (!document.getElementById)
    return;
  var b = document.getElementById ("balloon_help");
  if (b != null)
    b.style.display = "none";
}

/*
 * create the help bubble if need be and return it with the
 * help text embedded
 */
function createHelp (h)
{
  if (!document.getElementById)
    return;
  var c = null;
  var b = document.getElementById ("balloon_help");
  if (b == null)
  {
    b = document.createElement ("span");
    b.id = "balloon_help";
    b.style.position = "absolute";
    b.className = "tip";

    c = document.createElement ("span");
    c.className = "top";
    c.style.display = "block";
    b.appendChild (c);

    var d = document.createElement ("span");
    d.className = "bottom";
    d.style.display = "block";
    b.appendChild (d);

    document.body.appendChild (b);
  }
  else
  {
    c = b.firstChild;
    c.removeChild (c.firstChild);
  }
  c.appendChild (document.createTextNode (h));
  b.style.display = "block";
  return (b);
}

/*
 * move our help bubble next to the input field, which is normally
 * a child of the object moused over.  That made assigning the events
 * easier and also gave a bigger trigger area for the event.
 */

function moveHelp (o, bubble)
{
  if (!o)
    o = window.event;
  else
    o = o.firstChild;
  if (!o.offsetParent)
  {
    // alert ("browser doesn't support offsetParent");
    return;
  }
  var l = o.offsetWidth;
  var t = o.offsetHeight;
  do
  {
    l += o.offsetLeft;
    t += o.offsetTop;
  } while (o = o.offsetParent);

  bubble.style.top = (t - 15) + "px";
  bubble.style.left = (l - 40) + "px";
}

