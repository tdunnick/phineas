/*
 * stuff needed to support our configuration form and processing
 */
 
// find the ancestor matching this tag
function findAncestor (o, tag)
{
  while (o && (o.nodeName != tag))
	{
    if (o.parentNode)
		  o = o.parentNode;
  	else if (o.offsetParent)
	    o = o.offsetParent;
	}
	return (o);
}

// find the previous sibling matching this tag
function findPrevSibling (o, tag)
{
  while (o && (o.nodeName != tag))
	  o = o.previousSibling;
	return (o);
}

// find the next sibling matching this tag
function findNextSibling (o, tag)
{
  while (o && (o.nodeName != tag))
	  o = o.nextSibling;
	return (o);
}

// find the child matching this tag
function findChild (o, tag)
{
  if (o)
   o = findNextSibling (o.firstChild, tag);
	return (o);
}

// add a repeated field - assumes at least one parent TR with a 
// prompt and input in each of two TD elements.

function addfield (o) {
  var fname = o.name;
  o = findAncestor (o, "TR");
	// get the index of the previous row
	var p = findPrevSibling (o.previousSibling, "TR");
	p = findChild (p, "TD");
	var i = p.innerHTML.replace (new RegExp ("^\\s*" + fname), "") - 0;
  // create a new table row
	var c = document.createElement("TR");
	var d = document.createElement ("TD");
	// add the prompt
	d.appendChild (document.createTextNode (fname + " " + (i + 1)));
	c.appendChild (d);
  // get input entry
	p = findChild (findNextSibling (p.nextSibling, "TD"), "INPUT");
	var f = document.createElement ("INPUT");
	f.type = p.type;
	f.size = p.size;
	f.name = p.name.replace (new RegExp (fname + ".*\\]"), fname + "[" + i + "]");
	d = document.createElement ("TD");
	d.appendChild (f);
	c.appendChild (d);
  // finally place it in the form
  if (o.parentNode)
	  o.parentNode.insertBefore (c, o);
	else
	  o.offsetParent.insertBefore (c, o);
}

/*
 * our submit function...  
 * set the hidden request value to a name value pair to get around 
 * differences in how browser pass button values and to suppress
 * accidental submission by the Enter key and make user confirm
 * the submission.
 */

function setRequest (o, v)
{
  o.form.ConfigurationRequest.value = o.name + ":" + v;
	if (confirm (o.value))
	  o.form.submit ();
}