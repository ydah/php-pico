--TEST--
nested array copy-on-write vector 008
--FILE--
<?php
$tree = ['node' => ['value' => 33, 'items' => [34]]];
$copy = $tree;
$copy['node']['value'] += 3;
$copy['node']['items'][] = 38;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
33:36:34:34,38
