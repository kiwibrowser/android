// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_HANDLER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_export.h"

class PrefValueMap;

namespace policy {

class PolicyErrorMap;
struct PolicyHandlerParameters;
class PolicyMap;

// Maps a policy type to a preference path, and to the expected value type.
struct POLICY_EXPORT PolicyToPreferenceMapEntry {
  const char* const policy_name;
  const char* const preference_path;
  const base::Value::Type value_type;
};

// An abstract super class that subclasses should implement to map policies to
// their corresponding preferences, and to check whether the policies are valid.
class POLICY_EXPORT ConfigurationPolicyHandler {
 public:
  ConfigurationPolicyHandler();
  virtual ~ConfigurationPolicyHandler();

  // Returns whether the policy settings handled by this
  // ConfigurationPolicyHandler can be applied.  Fills |errors| with error
  // messages or warnings.  |errors| may contain error messages even when
  // |CheckPolicySettings()| returns true.
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) = 0;

  // Processes the policies handled by this ConfigurationPolicyHandler and sets
  // the appropriate preferences in |prefs|.
  virtual void ApplyPolicySettingsWithParameters(
      const PolicyMap& policies,
      const PolicyHandlerParameters& parameters,
      PrefValueMap* prefs);

  // Modifies the values of some of the policies in |policies| so that they
  // are more suitable to display to the user. This can be used to remove
  // sensitive values such as passwords, or to pretty-print values.
  virtual void PrepareForDisplaying(PolicyMap* policies) const;

 protected:
  // This is a convenience version of ApplyPolicySettingsWithParameters()
  // for derived classes that leaves out the |parameters|. Anyone extending
  // ConfigurationPolicyHandler should implement either
  // ApplyPolicySettingsWithParameters directly and implement
  // ApplyPolicySettings with a NOTREACHED or implement only
  // ApplyPolicySettings.
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyHandler);
};

// Abstract class derived from ConfigurationPolicyHandler that should be
// subclassed to handle a single policy (not a combination of policies).
class POLICY_EXPORT TypeCheckingPolicyHandler
    : public ConfigurationPolicyHandler {
 public:
  TypeCheckingPolicyHandler(const char* policy_name,
                            base::Value::Type value_type);
  ~TypeCheckingPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

  const char* policy_name() const;

 protected:
  // Runs policy checks and returns the policy value if successful.
  bool CheckAndGetValue(const PolicyMap& policies,
                        PolicyErrorMap* errors,
                        const base::Value** value);

 private:
  // The name of the policy.
  const char* policy_name_;

  // The type the value of the policy should have.
  base::Value::Type value_type_;

  DISALLOW_COPY_AND_ASSIGN(TypeCheckingPolicyHandler);
};

// Policy handler that makes sure the policy value is a list and filters out any
// list entries that are not of type |list_entry_type|. Derived methods may
// apply additional filters on list entries and transform the filtered list.
class POLICY_EXPORT ListPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  ListPolicyHandler(const char* policy_name, base::Value::Type list_entry_type);
  ~ListPolicyHandler() override;

  // TypeCheckingPolicyHandler methods:
  // Marked as final since overriding them could bypass filtering. Override
  // CheckListEntry() and ApplyList() instead.
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) final;

  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) final;

 protected:
  // Override this method to apply a filter for each |value| in the list.
  // |value| is guaranteed to be of type |list_entry_type_| at this point.
  // Returning false removes the value from |filtered_list| passed into
  // ApplyList(). By default, any value of type |list_entry_type_| is accepted.
  virtual bool CheckListEntry(const base::Value& value);

  // Implement this method to apply the |filtered_list| of values of type
  // |list_entry_type_| as returned from CheckAndGetList() to |prefs|.
  virtual void ApplyList(std::unique_ptr<base::ListValue> filtered_list,
                         PrefValueMap* prefs) = 0;

 private:
  // Checks whether the policy value is indeed a list, filters out all entries
  // that are not of type |list_entry_type_| or where CheckListEntry() returns
  // false, and returns the |filtered_list| if not nullptr. Sets errors for
  // filtered list entries if |errors| is not nullptr.
  bool CheckAndGetList(const policy::PolicyMap& policies,
                       policy::PolicyErrorMap* errors,
                       std::unique_ptr<base::ListValue>* filtered_list);

  // Expected value type for list entries. All other types are filtered out.
  base::Value::Type list_entry_type_;

  DISALLOW_COPY_AND_ASSIGN(ListPolicyHandler);
};

// Abstract class derived from TypeCheckingPolicyHandler that ensures an int
// policy's value lies in an allowed range. Either clamps or rejects values
// outside the range.
class POLICY_EXPORT IntRangePolicyHandlerBase
    : public TypeCheckingPolicyHandler {
 public:
  IntRangePolicyHandlerBase(const char* policy_name,
                            int min,
                            int max,
                            bool clamp);

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

 protected:
  ~IntRangePolicyHandlerBase() override;

  // Ensures that the value is in the allowed range. Returns false if the value
  // cannot be parsed or lies outside the allowed range and clamping is
  // disabled.
  bool EnsureInRange(const base::Value* input,
                     int* output,
                     PolicyErrorMap* errors);

 private:
  // The minimum value allowed.
  int min_;

  // The maximum value allowed.
  int max_;

  // Whether to clamp values lying outside the allowed range instead of
  // rejecting them.
  bool clamp_;

  DISALLOW_COPY_AND_ASSIGN(IntRangePolicyHandlerBase);
};

// ConfigurationPolicyHandler for policies that map directly to a preference.
class POLICY_EXPORT SimplePolicyHandler : public TypeCheckingPolicyHandler {
 public:
  SimplePolicyHandler(const char* policy_name,
                      const char* pref_path,
                      base::Value::Type value_type);
  ~SimplePolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // The DictionaryValue path of the preference the policy maps to.
  const char* pref_path_;

  DISALLOW_COPY_AND_ASSIGN(SimplePolicyHandler);
};

// Base class that encapsulates logic for mapping from a string enum list
// to a separate matching type value.
class POLICY_EXPORT StringMappingListPolicyHandler
    : public TypeCheckingPolicyHandler {
 public:
  // Data structure representing the map between policy strings and
  // matching pref values.
  class POLICY_EXPORT MappingEntry {
   public:
    MappingEntry(const char* policy_value, std::unique_ptr<base::Value> map);
    ~MappingEntry();

    const char* enum_value;
    std::unique_ptr<base::Value> mapped_value;
  };

  // Callback that generates the map for this instance.
  using GenerateMapCallback =
      base::Callback<void(std::vector<std::unique_ptr<MappingEntry>>*)>;

  StringMappingListPolicyHandler(const char* policy_name,
                                 const char* pref_path,
                                 const GenerateMapCallback& map_generator);
  ~StringMappingListPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Attempts to convert the list in |input| to |output| according to the table,
  // returns false on errors.
  bool Convert(const base::Value* input,
               base::ListValue* output,
               PolicyErrorMap* errors);

  // Helper method that converts from a policy value string to the associated
  // pref value.
  std::unique_ptr<base::Value> Map(const std::string& entry_value);

  // Name of the pref to write.
  const char* pref_path_;

  // The callback invoked to generate the map for this instance.
  GenerateMapCallback map_getter_;

  // Map of string policy values to local pref values. This is generated lazily
  // so the generation does not have to happen if no policy is present.
  std::vector<std::unique_ptr<MappingEntry>> map_;

  DISALLOW_COPY_AND_ASSIGN(StringMappingListPolicyHandler);
};

// A policy handler implementation that ensures an int policy's value lies in an
// allowed range.
class POLICY_EXPORT IntRangePolicyHandler : public IntRangePolicyHandlerBase {
 public:
  IntRangePolicyHandler(const char* policy_name,
                        const char* pref_path,
                        int min,
                        int max,
                        bool clamp);
  ~IntRangePolicyHandler() override;

  // ConfigurationPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Name of the pref to write.
  const char* pref_path_;

  DISALLOW_COPY_AND_ASSIGN(IntRangePolicyHandler);
};

// A policy handler implementation that maps an int percentage value to a
// double.
class POLICY_EXPORT IntPercentageToDoublePolicyHandler
    : public IntRangePolicyHandlerBase {
 public:
  IntPercentageToDoublePolicyHandler(const char* policy_name,
                                     const char* pref_path,
                                     int min,
                                     int max,
                                     bool clamp);
  ~IntPercentageToDoublePolicyHandler() override;

  // ConfigurationPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Name of the pref to write.
  const char* pref_path_;

  DISALLOW_COPY_AND_ASSIGN(IntPercentageToDoublePolicyHandler);
};

// Like TypeCheckingPolicyHandler, but validates against a schema instead of a
// single type. |schema| is the schema used for this policy, and |strategy| is
// the strategy used for schema validation errors.
class POLICY_EXPORT SchemaValidatingPolicyHandler
    : public ConfigurationPolicyHandler {
 public:
  SchemaValidatingPolicyHandler(const char* policy_name,
                                Schema schema,
                                SchemaOnErrorStrategy strategy);
  ~SchemaValidatingPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

  const char* policy_name() const;

 protected:
  // Runs policy checks and returns the policy value if successful.
  bool CheckAndGetValue(const PolicyMap& policies,
                        PolicyErrorMap* errors,
                        std::unique_ptr<base::Value>* output);

 private:
  const char* policy_name_;
  const Schema schema_;
  const SchemaOnErrorStrategy strategy_;

  DISALLOW_COPY_AND_ASSIGN(SchemaValidatingPolicyHandler);
};

// Maps policy to pref like SimplePolicyHandler while ensuring that the value
// set matches the schema. |schema| is the schema used for policies, and
// |strategy| is the strategy used for schema validation errors.
// The |recommended_permission| and |mandatory_permission| flags indicate the
// levels at which the policy can be set. A value set at an unsupported level
// will be ignored.
class POLICY_EXPORT SimpleSchemaValidatingPolicyHandler
    : public SchemaValidatingPolicyHandler {
 public:
  enum MandatoryPermission { MANDATORY_ALLOWED, MANDATORY_PROHIBITED };
  enum RecommendedPermission { RECOMMENDED_ALLOWED, RECOMMENDED_PROHIBITED };

  SimpleSchemaValidatingPolicyHandler(
      const char* policy_name,
      const char* pref_path,
      Schema schema,
      SchemaOnErrorStrategy strategy,
      RecommendedPermission recommended_permission,
      MandatoryPermission mandatory_permission);
  ~SimpleSchemaValidatingPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  const char* pref_path_;
  const bool allow_recommended_;
  const bool allow_mandatory_;

  DISALLOW_COPY_AND_ASSIGN(SimpleSchemaValidatingPolicyHandler);
};

// Maps policy to pref like SimplePolicyHandler while ensuring that the value
// is either a single JSON string or a list of JSON strings, and that the JSON,
// when parsed, matches the policy's validation_schema field found in |schema|.
// If |allow_errors_in_embedded_json| is true, then errors inside the JSON
// string only cause warnings, they do not cause validation to fail. However,
// the value as a whole is still validated by ensuring it is either a single
// string or a list of strings, whichever is appropriate.
// NOTE: Do not store new policies using JSON strings! If your policy has a
// complex schema, store it as a dict of that schema. This has some advantages:
// - You don't have to parse JSON every time you read it from the pref store.
// - Nested dicts are simple, but nested JSON strings are complicated.
class POLICY_EXPORT SimpleJsonStringSchemaValidatingPolicyHandler
    : public ConfigurationPolicyHandler {
 public:
  SimpleJsonStringSchemaValidatingPolicyHandler(
      const char* policy_name,
      const char* pref_path,
      Schema schema,
      SchemaOnErrorStrategy strategy,
      SimpleSchemaValidatingPolicyHandler::RecommendedPermission
          recommended_permission,
      SimpleSchemaValidatingPolicyHandler::MandatoryPermission
          mandatory_permission,
      bool allow_errors_in_embedded_json);

  ~SimpleJsonStringSchemaValidatingPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Validates |root_value| as a single JSON string that matches the schema.
  bool CheckSingleJsonString(const base::Value* root_value,
                             PolicyErrorMap* errors);

  // Validates |root_value| as a list of JSON strings that match the schema.
  bool CheckListOfJsonStrings(const base::Value* root_value,
                              PolicyErrorMap* errors);

  // Validates that the given JSON string matches the schema. |index| is used
  // only in error messages, it is the index of the given string in the list
  // if the root value is a list, and ignored otherwise. Adds any errors it
  // finds to |errors|.
  bool ValidateJsonString(const std::string& json_string,
                          PolicyErrorMap* errors,
                          int index);

  // Returns a string describing where an error occurred - |index| is the index
  // of the string where the error occurred if the root value is a list, and
  // ignored otherwise. |json_error_path| describes where the error occurred
  // inside a JSON string (this can be empty).
  std::string ErrorPath(int index, std::string json_error_path);

  // Record to UMA that this policy failed validation due to an error in one or
  // more embedded JSON strings - either unparsable, or didn't match the schema.
  void RecordJsonError();

  // Returns true if the schema root is a list.
  inline bool IsListSchema() {
    return schema_.type() == base::Value::Type::LIST;
  }

 private:
  const char* policy_name_;
  const Schema schema_;
  const SchemaOnErrorStrategy strategy_;
  const char* pref_path_;
  const bool allow_recommended_;
  const bool allow_mandatory_;
  bool allow_errors_in_embedded_json_;

  DISALLOW_COPY_AND_ASSIGN(SimpleJsonStringSchemaValidatingPolicyHandler);
};

// A policy handler to deprecate multiple legacy policies with a new one.
// This handler will completely ignore any of legacy policy values if the new
// one is set.
class POLICY_EXPORT LegacyPoliciesDeprecatingPolicyHandler
    : public ConfigurationPolicyHandler {
 public:
  LegacyPoliciesDeprecatingPolicyHandler(
      std::vector<std::unique_ptr<ConfigurationPolicyHandler>>
          legacy_policy_handlers,
      std::unique_ptr<SchemaValidatingPolicyHandler> new_policy_handler);
  ~LegacyPoliciesDeprecatingPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettingsWithParameters(
      const policy::PolicyMap& policies,
      const policy::PolicyHandlerParameters& parameters,
      PrefValueMap* prefs) override;

 protected:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  std::vector<std::unique_ptr<ConfigurationPolicyHandler>>
      legacy_policy_handlers_;
  std::unique_ptr<SchemaValidatingPolicyHandler> new_policy_handler_;

  DISALLOW_COPY_AND_ASSIGN(LegacyPoliciesDeprecatingPolicyHandler);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_HANDLER_H_
