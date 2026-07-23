--TEST--
nested array copy-on-write vector 013
--FILE--
<?php
$tree = ['node' => ['value' => 53, 'items' => [54]]];
$copy = $tree;
$copy['node']['value'] += 8;
$copy['node']['items'][] = 63;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
53:61:54:54,63
