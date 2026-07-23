--TEST--
nested array copy-on-write vector 003
--FILE--
<?php
$tree = ['node' => ['value' => 13, 'items' => [14]]];
$copy = $tree;
$copy['node']['value'] += 5;
$copy['node']['items'][] = 20;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
13:18:14:14,20
