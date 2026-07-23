--TEST--
nested array copy-on-write vector 010
--FILE--
<?php
$tree = ['node' => ['value' => 41, 'items' => [42]]];
$copy = $tree;
$copy['node']['value'] += 5;
$copy['node']['items'][] = 48;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
41:46:42:42,48
