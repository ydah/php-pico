--TEST--
nested array copy-on-write vector 029
--FILE--
<?php
$tree = ['node' => ['value' => 117, 'items' => [118]]];
$copy = $tree;
$copy['node']['value'] += 3;
$copy['node']['items'][] = 122;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
117:120:118:118,122
