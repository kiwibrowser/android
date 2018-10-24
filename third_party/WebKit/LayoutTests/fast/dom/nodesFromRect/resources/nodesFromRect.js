/*
 * Contributors:
 *     * Antonio Gomes <tonikitoo@webkit.org>
 *     * Allan Sandfeld Jensen <allan.jensen@digia.com>
**/

function nodeToString(node) {
    var str = "";
    if (node.nodeType == Node.ELEMENT_NODE) {
        str += node.nodeName;
        if (node.id)
            str += '#' + node.id;
        else if (node.class)
            str += '.' + node.class;
    } else if (node.nodeType == Node.TEXT_NODE) {
        str += "'" + node.data + "'";
    } else if (node.nodeType == Node.DOCUMENT_NODE) {
        str += "#document";
    }
    return str;
}

function nodeListToString(nodes) {
    var nodeString = "";

    for (var i = 0; i < nodes.length; i++) {
        var str = nodeToString(nodes[i]);
        if (!str)
            continue;
        nodeString += str;
        if (i + 1 < nodes.length)
            nodeString += ", ";
    }
    return nodeString;
}

function check(x, y, width, height, list, doc)
{
  if (!window.internals)
    return;

  if (!doc)
    doc = document;

    var nodes = internals.nodesFromRect(doc, x, y, width, height, true /* ignoreClipping */, false /* allow child-frame content */);
  if (!nodes)
    return;

  if (nodes.length != list.length) {
    testFailed("Different number of nodes for rect" +
              "[" + x + "," + y + "], " +
              "[" + width + "," + height +
              "]: '" + list.length + "' vs '" + nodes.length +
              "', found: " + nodeListToString(nodes));
    return;
  }

  for (var i = 0; i < nodes.length; i++) {
    if (nodes[i] != list[i]) {
      testFailed("Unexpected node #" + i + " for rect " +
                "[" + x + "," + y + "], " +
                "[" + width + "," + height + "]" +
                 " - " + nodeToString(nodes[i]));
      return;
    }
  }

  testPassed("All correct nodes found for rect");
}

function checkRect(left, top, width, height, expectedNodeString, doc)
{
    if (!window.internals)
        return;

    if (height <=0 || width <= 0)
        return;

    if (!doc)
        doc = document;

    var nodeString = nodesFromRectAsString(doc, left, top, width, height);

    if (nodeString == expectedNodeString) {
        testPassed("All correct nodes found for rect");
    } else {
        testFailed("NodesFromRect should be [" + expectedNodeString + "] was [" + nodeString + "]");
    }
}

function checkPoint(left, top, expectedNodeString, doc)
{
    if (!window.internals)
        return;

    if (!doc)
        doc = document;

    var nodeString = nodesFromRectAsString(doc, left, top, 1, 1);

    if (nodeString == expectedNodeString) {
        testPassed("Correct node found for point");
    } else {
        testFailed("NodesFromRect should be [" + expectedNodeString + "] was [" + nodeString + "]");
    }
}

function nodesFromRectAsString(doc, x, y, width, height)
{
    var nodes = internals.nodesFromRect(doc, x, y, width, height, true /* ignoreClipping */, true /* allow child-frame content */);
    if (!nodes)
        return "";

    return nodeListToString(nodes);
}

function getCenterFor(element)
{
  var rect = element.getBoundingClientRect();
  return { x : parseInt((rect.left + rect.right) / 2) , y : parseInt((rect.top + rect.bottom) / 2)};
}

function getTopFor(element)
{
  var rect = element.getBoundingClientRect();
  return { x : parseInt((rect.left + rect.right) / 2) , y : parseInt(rect.top)};
}

function getBottomFor(element)
{
  var rect = element.getBoundingClientRect();
  return { x : parseInt((rect.left + rect.right) / 2) , y : parseInt(rect.bottom)};
}
